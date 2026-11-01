// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
"use strict";
// Privileged Gecko host only. None of these APIs are exposed to web pages.
const {Downloads}=ChromeUtils.importESModule("resource://gre/modules/Downloads.sys.mjs");
const {DownloadIntegration}=ChromeUtils.importESModule("resource://gre/modules/DownloadIntegration.sys.mjs");
const {AddonManager,AddonManagerPrivate}=ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");
const {FileUtils}=ChromeUtils.importESModule("resource://gre/modules/FileUtils.sys.mjs");
const permissionLabels={geo:"Localisation","desktop-notification":"Notifications",camera:"Caméra",microphone:"Microphone",
 "persistent-storage":"Stockage persistant","clipboard-read":"Lecture du presse-papiers","clipboard-write":"Écriture du presse-papiers",
 "screen-wake-lock":"Maintien de l’écran allumé",midi:"MIDI","midi-sysex":"MIDI système","speaker-selection":"Sortie audio",
 "autoplay-media":"Lecture automatique",popup:"Fenêtres surgissantes",cookie:"Cookies","storage-access":"Stockage intersite",
 xr:"Réalité virtuelle / augmentée","local-network":"Réseau local","loopback-network":"Services de cet appareil"};
const permissionTypes=new Set(Object.keys(permissionLabels)),pendingPrompts=new Map(),downloads=new Map(),nativeFactories=[];
let promptId=1,downloadId=1,downloadList,downloadTimer;
function publish(category,data){send({type:"service-state",category,data});}
function tabForBrowser(browser){for(const tab of tabs.values())if(tab.browser===browser)return tab.id;return 0;}
function ask(details,decide){const id=promptId++,timer=setTimeout(()=>finishPrompt(id,{allow:false}),120000);
 pendingPrompts.set(id,{details:{id,...details},decide,timer});publishPrompts();return id;}
function publishPrompts(){publish("prompts",[...pendingPrompts.values()].map(p=>p.details));}
function finishPrompt(id,answer){const p=pendingPrompts.get(id);if(!p)return;pendingPrompts.delete(id);clearTimeout(p.timer);
 try{p.decide(answer);}catch(e){publish("error",String(e));}publishPrompts();}
function cancelTabPrompts(id){for(const [key,p] of pendingPrompts)if(p.details.tab===id)finishPrompt(key,{allow:false});}
window.CyUriumMediaRequest=function(actor,browser,data,principal){
 const groups=[];if(data.videoInputDevices?.length)groups.push({label:data.sharingScreen?"Écran ou fenêtre à partager":"Caméra",choices:data.videoInputDevices});
 if(data.audioInputDevices?.length)groups.push({label:"Microphone",choices:data.audioInputDevices});
 if(data.audioOutputDevices?.length)groups.push({label:"Sortie audio",choices:data.audioOutputDevices});
 const types=[];if(data.videoInputDevices?.length&&!data.sharingScreen)types.push("camera");if(data.audioInputDevices?.length&&!data.sharingAudio)types.push("microphone");
 const reply=answer=>{
  const selected=answer.devices||groups.map(g=>g.choices[0].deviceIndex);
  const valid=groups.every(g=>g.choices.some(d=>selected.includes(d.deviceIndex)));
  if(answer.remember&&!data.sharingScreen&&!data.sharingAudio)for(const type of types)Services.perms.addFromPrincipal(principal,type,answer.allow?1:2);
  actor.sendAsyncMessage(answer.allow&&valid?"webrtc:Allow":"webrtc:Deny",{callID:data.callID,windowID:data.windowID,devices:selected});permissionsSnapshot();
 };
 const stored=types.map(t=>Services.perms.testPermissionFromPrincipal(principal,t));
 if(stored.some(v=>v===2)){reply({allow:false});return;}
 if(types.length&&stored.every(v=>v===1)&&!data.sharingScreen&&!data.sharingAudio){reply({allow:true});return;}
 ask({kind:"media",tab:tabForBrowser(browser),callID:data.callID,origin:principal.origin,title:data.sharingScreen?"Partager un écran ou une fenêtre":"Accès aux périphériques multimédias",
  devices:groups,remember:!data.sharingScreen&&!data.sharingAudio},reply);
};
window.CyUriumMediaCancel=function(callID){for(const [id,p] of pendingPrompts)if(p.details.callID===callID)finishPrompt(id,{allow:false});};
const mediaIndicators=new Map(),mediaActors=new Map();
window.CyUriumMediaIndicator=function(browser,data,actor){
 if(data.remove){mediaIndicators.delete(data.windowID);mediaActors.delete(data.windowID);}
 else {mediaIndicators.set(data.windowID,{tab:tabForBrowser(browser),...data});mediaActors.set(data.windowID,actor);}
 publish("media",[...mediaIndicators.values()]);
};
function stopMedia(tab,origin){for(const [id,data] of mediaIndicators)if((!tab||data.tab===tab)&&(!origin||data.origin===origin))try{
 const actor=mediaActors.get(id);if(data.screen||data.window||data.browser)actor?.sendAsyncMessage("webrtc:StopSharing","screen:"+id);
 if(data.camera||data.microphone)actor?.sendAsyncMessage("webrtc:StopSharing",id);
 }catch{}}
function registerService(contract,name,make){const registrar=Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
 const factory={QueryInterface:ChromeUtils.generateQI(["nsIFactory"]),createInstance(iid){if(Services.env.get("CYURIUM_GECKO_TEST")==="1")publish("diagnostic","Factory "+contract);return make().QueryInterface(iid);}};
 registrar.registerFactory(Services.uuid.generateUUID(),name,contract,factory);nativeFactories.push(factory);}
function permissionsSnapshot(){const result=[];for(const p of Services.perms.all)if(permissionTypes.has(p.type))result.push({origin:p.principal.origin,type:p.type,label:permissionLabels[p.type]||p.type,value:p.capability});publish("permissions",result);}
function promptPermission(request){
 if(Services.env.get("CYURIUM_GECKO_TEST")==="1")publish("diagnostic","Permission request received");
 const principal=request.principal,types=[];for(let i=0;i<request.types.length;i++){const item=request.types.queryElementAt(i,Ci.nsIContentPermissionType),type=item.type;
  types.push(type==="geolocation"?"geo":type==="midi"&&item.options?.length&&String(item.options.queryElementAt(0,Ci.nsISupportsString))==="sysex"?"midi-sysex":type);}
 if(!types.length||types.some(t=>!permissionTypes.has(t))){request.cancel();publish("notice","Demande refusée : type de permission non pris en charge ("+types.join(", ")+").");return;}
 const browser=request.element||request.browsingContext?.top.embedderElement;
 const values=types.map(t=>Services.perms.testPermissionFromPrincipal(principal,t));
 if(values.some(v=>v===2)){request.cancel();return;}if(values.every(v=>v===1)){request.allow();return;}
 ask({kind:"permission",tab:tabForBrowser(browser),origin:principal.origin,title:types.map(t=>permissionLabels[t]||t).join(", "),remember:true},answer=>{
  if(answer.remember)for(const type of types)Services.perms.addFromPrincipal(principal,type,answer.allow?1:2);
  if(answer.allow)request.allow();else request.cancel();permissionsSnapshot();
 });
}
async function publishDownloads(){if(!downloadList)return;const list=await downloadList.getAll();
 publish("downloads",list.map(d=>{let id=[...downloads].find(([,item])=>item===d)?.[0];if(!id){id=downloadId++;downloads.set(id,d);}
  return {id,name:PathUtils.filename(d.target.path),path:d.target.path,url:d.source.url,received:d.currentBytes,total:d.totalBytes,
   progress:d.progress,speed:d.speed,stopped:d.stopped,succeeded:d.succeeded,canceled:d.canceled,partial:d.hasPartialData,
   blocked:!!d.error?.becauseBlocked,error:d.error?String(d.error.message):""};}).reverse());}
function downloadsChanged(){if(!downloadTimer)downloadTimer=setTimeout(()=>{downloadTimer=null;publishDownloads().catch(console.error);},180);}
async function downloadTarget(name){const folder=await DownloadIntegration.getPreferredDownloadsDirectory();await IOUtils.makeDirectory(folder,{ignoreExisting:true});
 const {DownloadPaths}=ChromeUtils.importESModule("resource://gre/modules/DownloadPaths.sys.mjs");
 const file=new FileUtils.File(PathUtils.join(folder,DownloadPaths.sanitize(name||"download")));file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE,0o600);return file;}
async function extensionsSnapshot(){const list=await AddonManager.getAddonsByTypes(["extension"]);
 publish("extensions",list.filter(a=>!a.isSystem&&!a.hidden).map(a=>({id:a.id,name:a.name,version:a.version,description:a.description||"",
  active:a.isActive,disabled:a.userDisabled,incompatible:a.appDisabled,signed:a.signedState>AddonManager.SIGNEDSTATE_MISSING,permissions:a.userPermissions||{},options:a.optionsURL||""})));}
async function installExtension(path){
 const file=new FileUtils.File(path);if(!file.exists()||!file.isFile()||!path.toLowerCase().endsWith(".xpi"))throw new Error("Sélectionnez un fichier .xpi signé par Mozilla.");
 const install=await AddonManager.getInstallForFile(file);if(install.error)throw new Error("Extension refusée par Gecko ("+install.error+").");
 const addon=install.addon;if(!addon||addon.signedState<=AddonManager.SIGNEDSTATE_MISSING)throw new Error("Signature Mozilla absente ou invalide.");
 if(addon.appDisabled)throw new Error("Cette extension est incompatible avec cette version de Gecko.");
 const permissions=addon.userPermissions||{};
 ask({kind:"extension",title:"Installer « "+addon.name+" » ?",origin:addon.id,permissions:[...(permissions.permissions||[]),...(permissions.origins||[])],remember:false},answer=>{
  if(!answer.allow){install.cancel();return;}
  install.addListener({onInstallEnded(){extensionsSnapshot();publish("notice","Extension installée : "+addon.name);},onInstallFailed(){publish("error","Installation échouée ("+install.error+").");}});
  install.install().catch(e=>publish("error",String(e)));
 });
}
async function updateExtension(addon){
 const response=await fetch("https://addons.mozilla.org/api/v5/addons/addon/"+encodeURIComponent(addon.id)+"/",{credentials:"omit",signal:AbortSignal.timeout(30000)});
 if(!response.ok)throw new Error("Mise à jour de l’extension indisponible (Mozilla HTTP "+response.status+").");
 const data=await response.json(),latest=data.current_version;
 if(!latest?.file?.url)throw new Error("Aucune version disponible dans le catalogue Mozilla.");
 if(Services.vc.compare(latest.version,addon.version)<=0){publish("notice",addon.name+" est à jour.");return;}
 const url=new URL(latest.file.url);if(url.protocol!=="https:"||url.hostname!=="addons.mozilla.org")throw new Error("Source de mise à jour inattendue.");
 const folder=PathUtils.join(Services.env.get("MOZ_APP_DATA"),"extension-updates");await IOUtils.makeDirectory(folder,{ignoreExisting:true});
 const target=PathUtils.join(folder,Services.uuid.generateUUID().toString().replace(/[{}]/g,"")+".xpi");
 const download=await Downloads.createDownload({source:latest.file.url,target});await download.start();await installExtension(target);
}
function extensionPermission(subject){const {info}=subject.wrappedJSObject;
 ask({kind:"extension",title:"Autorisations de « "+info.addon.name+" »",origin:info.addon.id,
  permissions:[...(info.permissions?.permissions||[]),...(info.permissions?.origins||[])],remember:false},answer=>answer.allow?info.resolve():info.reject());}
function graphicsSnapshot(){try{
 const gfx=Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);let compositor="En attente d’une page web";
 for(const tab of tabs.values())try{compositor=tab.window.windowUtils.layerManagerType;break;}catch{}
 publish("graphics",{enabled:!Services.prefs.getBoolPref("layers.acceleration.disabled",false),compositor,adapter:gfx.adapterDescription,
  driver:gfx.adapterDriverVersion,features:gfx.getFeatureLog().features||[],hardwareVideo:Services.prefs.getBoolPref("media.hardware-video-decoding.enabled",true)});
 }catch(e){publish("graphics",{error:String(e)});}}
async function initServices(){
 initSecurity();
 ChromeUtils.importESModule("resource://gre/modules/ActorManagerParent.sys.mjs");
 if(Services.env.get("CYURIUM_GECKO_TEST")==="1"){Services.prefs.setBoolPref("media.navigator.streams.fake",true);Services.prefs.setBoolPref("media.navigator.permission.fake",true);}
 if(Services.env.get("CYURIUM_GECKO_TEST")==="1"){Services.prefs.setBoolPref("browser.dom.window.dump.enabled",true);Services.console.registerListener({observe(message){dump(String(message.message)+"\n");}});}
 try{ChromeUtils.unregisterWindowActor("WebRTC");}catch{}
 ChromeUtils.registerWindowActor("WebRTC",{parent:{esModuleURI:"chrome://cyurium/content/MediaParent.sys.mjs"},child:{esModuleURI:"resource:///actors/WebRTCChild.sys.mjs"},allFrames:true,includeChrome:false,messageManagerGroups:["cyurium"]});
 ChromeUtils.registerProcessActor("CyUriumMedia",{child:{esModuleURI:"chrome://cyurium/content/MediaProcess.sys.mjs",observers:["getUserMedia:request","recording-device-stopped","PeerConnection:request","recording-device-events","recording-window-ended"]}});
 registerService("@mozilla.org/content-permission/prompt;1","CyUrium permissions",()=>({QueryInterface:ChromeUtils.generateQI(["nsIContentPermissionPrompt"]),prompt:promptPermission}));
 registerService("@mozilla.org/helperapplauncherdialog;1","CyUrium downloads",()=>({QueryInterface:ChromeUtils.generateQI(["nsIHelperAppLauncherDialog"]),
  show(launcher){launcher.MIMEInfo.preferredAction=Ci.nsIMIMEInfo.saveToDisk;launcher.saveToDisk(null,false);},
  promptForSaveToFileAsync(launcher,context,name){downloadTarget(name).then(file=>launcher.saveDestinationAvailable(file)).catch(e=>{launcher.cancel(Cr.NS_BINDING_ABORTED);publish("error",String(e));});}}));
 downloadList=await Downloads.getList(Downloads.ALL);await downloadList.addView({onDownloadAdded:downloadsChanged,onDownloadChanged:downloadsChanged,onDownloadRemoved:downloadsChanged});await publishDownloads();
 if(!AddonManager.isReady)AddonManagerPrivate.startup();
 await AddonManager.readyPromise;
 // Gecko embedders do not run Firefox's SessionStore startup sequence.
 const {ExtensionParent}=ChromeUtils.importESModule("resource://gre/modules/ExtensionParent.sys.mjs");
 await ExtensionParent.apiManager.lazyInit();
 const adapter=await ChromeUtils.compileScript("chrome://cyurium/content/ext-workspace.js");adapter.executeInGlobal(ExtensionParent.apiManager.global);
 await Promise.resolve();
 ExtensionParent.browserPaintedPromise=Promise.resolve();Services.obs.notifyObservers(window,"extensions-late-startup");
 AddonManager.addAddonListener({onInstalled:extensionsSnapshot,onUninstalled:extensionsSnapshot,onEnabled:extensionsSnapshot,onDisabled:extensionsSnapshot});
 Services.obs.addObserver({observe:extensionPermission},"webextension-permission-prompt");
 Services.obs.addObserver({observe(subject){extensionPermission({wrappedJSObject:{info:subject.wrappedJSObject}});}},"webextension-update-permission-prompt");
 Services.obs.addObserver({observe:permissionsSnapshot},"perm-changed");
 try{await ensureBundledExtensions();}catch(error){console.error(error);publish("error","Installation du bloqueur de publicités : "+String(error));}
 permissionsSnapshot();await extensionsSnapshot();graphicsSnapshot();await initUpdates();publish("ready",true);
}
async function serviceCommand(m){
 if(m.type==="prompt-answer"){finishPrompt(m.id,m);return true;}
 if(m.type==="permission-set"){
  if(!permissionTypes.has(m.permission)||![0,1,2,3].includes(m.value))throw new Error("Permission invalide");
  const principal=Services.scriptSecurityManager.createContentPrincipal(Services.io.newURI(m.origin),{});
  if(m.value)Services.perms.addFromPrincipal(principal,m.permission,m.value);else Services.perms.removeFromPrincipal(principal,m.permission);
  if(m.value!==1&&["camera","microphone"].includes(m.permission))stopMedia(0,principal.origin);
  permissionsSnapshot();return true;
 }
 if(m.type==="permissions-clear"){for(const p of Services.perms.all)if(permissionTypes.has(p.type))Services.perms.removeFromPrincipal(p.principal,p.type);stopMedia();permissionsSnapshot();return true;}
 if(m.type==="download-action"){
  const d=downloads.get(m.id);if(!d)return true;
  if(m.action==="pause"||m.action==="cancel"){await d.cancel();if(m.action==="cancel")await d.removePartialData();}
  else if(m.action==="resume"||m.action==="retry"){if(d.error?.becauseBlocked)throw new Error("Ce fichier est bloqué par la protection de Gecko.");d.start().catch(downloadsChanged);}
  else if(m.action==="open"&&d.succeeded)await d.launch();else if(m.action==="reveal")await DownloadIntegration.showContainingDirectory(d.target.path);
  else if(m.action==="remove"&&d.stopped){await downloadList.remove(d);await d.finalize(true);downloads.delete(m.id);}
  await publishDownloads();return true;
 }
 if(m.type==="extension-install"){await installExtension(m.path);return true;}
 if(m.type==="extension-options"||m.type==="extension-popup"){
  const addon=await AddonManager.getAddonByID(m.id);if(!addon?.isActive)return true;
  let url=addon.optionsURL;
  if(m.type==="extension-popup"){const policy=WebExtensionPolicy.getByID(m.id),extension=policy?.extension;
   const action=extension?.manifest.action||extension?.manifest.browser_action;url=action?.default_popup?extension.getURL(action.default_popup):"";
  }
  if(url&&m.type==="extension-popup"){
   const tab=tabs.get(m.tab)||[...tabs.values()][0],policy=WebExtensionPolicy.getByID(m.id);
   const options={title:addon.name,url,principal:policy.extension.principal,windowId:tab?.window.cyuriumWindowId||0};
   window.openDialog("chrome://cyurium/content/panel.xhtml","_blank","chrome,dialog=no,resizable=yes,width=480,height=620",{wrappedJSObject:options});
  }else if(url)send({type:"new-tab",opener:m.tab||[...tabs.keys()][0]||0,url});else publish("notice","Cette extension n’a pas de panneau à ouvrir.");return true;
 }
 if(m.type==="extension-pick"){
  const picker=Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  picker.init(([...tabs.values()][0]?.window||window).browsingContext,"Installer une extension signée",Ci.nsIFilePicker.modeOpen);picker.appendFilter("WebExtension (*.xpi)","*.xpi");
  picker.open({done(result){if(result===Ci.nsIFilePicker.returnOK)installExtension(picker.file.path).catch(e=>publish("error",String(e)));}});return true;
 }
 if(m.type==="extension-action"){
  const addon=await AddonManager.getAddonByID(m.id);if(!addon||addon.isSystem)return true;
  if(m.action==="enable")await addon.enable();else if(m.action==="disable")await addon.disable();else if(m.action==="remove")await addon.uninstall();
  else if(m.action==="update")updateExtension(addon).catch(e=>publish("error",String(e)));
  await extensionsSnapshot();return true;
 }
 if(m.type==="media-stop"){
  stopMedia(m.tab);return true;
 }
 if(m.type==="graphics-refresh"){graphicsSnapshot();return true;}
 if(m.type==="services-refresh"){await publishDownloads();await extensionsSnapshot();permissionsSnapshot();graphicsSnapshot();return true;}
 return false;
}
