// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
"use strict";
let updateChannel,verifiedUpdate,updateBusy=false,updateRetryAt=0,updateResult;
const updateRoot=PathUtils.join(Services.env.get("MOZ_APP_DATA"),"updates");
const updateTest=Services.env.get("CYURIUM_GECKO_TEST")==="1"&&!!Services.env.get("CYURIUM_UPDATE_TEST_CONFIG");
const updatePlatform="windows-x64",updateVersion=/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;
function updateStatus(status,message,extra={}){
 publish("update",{status,message,provider:updateChannel?.provider||"manifest",repository:updateChannel?.repository||"",channel:updateChannel?.channel||"stable",last_result:updateResult||null,...extra});
}
function decode64(s){return Uint8Array.from(atob(s),c=>c.charCodeAt(0));}
function httpsURL(value){
 const url=new URL(value),fixture=updateTest&&url.protocol==="http:"&&url.hostname==="127.0.0.1";
 if((url.protocol!=="https:"&&!fixture)||url.username||url.password||url.hash)throw new Error("L’URL de mise à jour doit utiliser HTTPS, sans identifiants.");return url;
}
function channelConfigured(){return !!(updateChannel?.public_key_spki&&(updateChannel.provider==="github"?updateChannel.repository:updateChannel.manifest_url));}
async function clearUpdateArchives(){
 try{for(const file of await IOUtils.getChildren(updateRoot))if(/^package(?:-[a-f0-9-]+)?\.zip$/i.test(PathUtils.filename(file))){
  try{await IOUtils.remove(file,{ignoreAbsent:true});}catch{}
 }}catch{}
}
// Keep normal TLS validation and block downgrades on every redirect, including
// package downloads. Retain Gecko's existing progress/auth interfaces.
function guardUpdateChannel(channel){
 const previous=channel.notificationCallbacks;let redirects=0;
 const callbacks={QueryInterface:ChromeUtils.generateQI(["nsIInterfaceRequestor","nsIChannelEventSink"]),
  getInterface(iid){if(iid.equals(Ci.nsIChannelEventSink))return this;if(previous)return previous.getInterface(iid);throw Cr.NS_ERROR_NO_INTERFACE;},
  asyncOnChannelRedirect(oldChannel,newChannel,flags,callback){
   try{httpsURL(newChannel.URI.spec);if(++redirects>8)throw new Error("Trop de redirections");}
   catch{callback.onRedirectVerifyCallback(Cr.NS_ERROR_ABORT);return;}
   let delegate;try{delegate=previous?.getInterface(Ci.nsIChannelEventSink);}catch{}
   if(delegate)delegate.asyncOnChannelRedirect(oldChannel,newChannel,flags,callback);else callback.onRedirectVerifyCallback(Cr.NS_OK);
  }};
 channel.notificationCallbacks=callbacks;channel.loadFlags|=Ci.nsIRequest.LOAD_ANONYMOUS|Ci.nsIRequest.LOAD_BYPASS_CACHE|Ci.nsIRequest.INHIBIT_CACHING;
}
function requestUpdateJSON(url,limit=65536,api=false){
 return new Promise((resolve,reject)=>{
  const xhr=new XMLHttpRequest({mozAnon:true});xhr.open("GET",httpsURL(url));xhr.timeout=30000;xhr.responseType="text";
  xhr.setRequestHeader("Accept",api?"application/vnd.github+json":"application/json");
  if(api)xhr.setRequestHeader("X-GitHub-Api-Version","2026-03-10");
  guardUpdateChannel(xhr.channel);
  xhr.onprogress=e=>{if(e.loaded>limit){reject(new Error("Réponse de mise à jour trop grande."));xhr.abort();}};
  xhr.onerror=()=>reject(new Error("Connexion de mise à jour impossible (réseau, certificat ou redirection)."));
  xhr.ontimeout=()=>reject(new Error("Le serveur de mise à jour ne répond pas."));
  xhr.onabort=()=>reject(new Error("Requête de mise à jour interrompue."));
  xhr.onload=()=>{try{
   httpsURL(xhr.responseURL);
   if(xhr.status===403||xhr.status===429){
    const reset=Number(xhr.getResponseHeader("X-RateLimit-Reset"))*1000,retry=Number(xhr.getResponseHeader("Retry-After"))*1000;
    updateRetryAt=Math.max(Date.now()+60000,Number.isFinite(reset)?reset:0,Date.now()+(Number.isFinite(retry)&&retry>0?retry:3600000));
    throw new Error("GitHub limite temporairement les requêtes. Réessayez après "+new Date(updateRetryAt).toLocaleTimeString()+".");
   }
   if(xhr.status===404&&api)throw new Error("Aucune Release accessible : dépôt absent, privé ou sans version publiée.");
   if(xhr.status!==200)throw new Error("Serveur de mise à jour : HTTP "+xhr.status);
   if(xhr.responseText.length>limit)throw new Error("Réponse de mise à jour trop grande.");
   resolve(JSON.parse(xhr.responseText));
  }catch(e){reject(e);}};xhr.send();
 });
}
function githubAssetURL(release,asset){
 const url=httpsURL(asset.browser_download_url);
 const base="/"+updateChannel.repository+"/releases/download/",suffix=encodeURIComponent(release.tag_name)+"/"+encodeURIComponent(asset.name);
 if(!(updateTest&&url.hostname==="127.0.0.1")&&(url.origin!=="https://github.com"||url.pathname.slice(0,base.length).toLowerCase()!==base.toLowerCase()||url.pathname.slice(base.length)!==suffix||url.search))throw new Error("Adresse d’asset GitHub incohérente.");
 return url.href;
}
async function githubRelease(){
 const origin=updateTest&&updateChannel.test_api_url?httpsURL(updateChannel.test_api_url).origin:"https://api.github.com";
 const endpoint=origin+"/repos/"+updateChannel.repository+"/releases";
 const result=await requestUpdateJSON(endpoint+(updateChannel.channel==="preview"?"?per_page=30":"/latest"),1048576,true);
 const eligible=r=>r&&r.draft===false&&(r.prerelease===false||updateChannel.channel==="preview")&&updateVersion.test(String(r.tag_name||"").replace(/^v/,""))&&Array.isArray(r.assets);
 const candidates=(Array.isArray(result)?result:[result]).filter(eligible).sort((a,b)=>Services.vc.compare(b.tag_name.replace(/^v/,""),a.tag_name.replace(/^v/,"")));
 for(const release of candidates){
  const assets=release.assets.filter(a=>a.name===updateChannel.manifest_asset&&a.state==="uploaded");
  if(assets.length!==1)continue;
  if(!Number.isSafeInteger(assets[0].size)||assets[0].size<=0||assets[0].size>65536)throw new Error("Manifeste GitHub trop grand ou invalide.");
  return {release,envelope:await requestUpdateJSON(githubAssetURL(release,assets[0]))};
 }
 throw new Error("Aucune Release compatible ne contient "+updateChannel.manifest_asset+".");
}
async function stagedUpdate(){
 try{
  const item=JSON.parse(await IOUtils.readUTF8(PathUtils.join(updateRoot,"staged.json")));
  if(updateVersion.test(item.version)&&Services.vc.compare(item.version,Services.appinfo.version)>0&&PathUtils.parent(item.stage)===updateRoot&&PathUtils.filename(item.stage).startsWith("payload-")&&await IOUtils.exists(item.stage))return item;
 }catch{}return null;
}
async function initUpdates(){
 try{
  const engine=Services.dirsvc.get("GreD",Ci.nsIFile),testConfig=updateTest?Services.env.get("CYURIUM_UPDATE_TEST_CONFIG"):"";
  try{updateResult=JSON.parse(await IOUtils.readUTF8(PathUtils.join(PathUtils.parent(Services.env.get("MOZ_APP_DATA")),"update-result.json")));}catch{}
  updateChannel=JSON.parse(await IOUtils.readUTF8(testConfig||PathUtils.join(engine.parent.path,"assets","update-channel.json")));
  await clearUpdateArchives();
  if(!channelConfigured()){updateStatus("unconfigured","Le dépôt GitHub Releases et la clé publique de publication restent à configurer.");return;}
  if((updateChannel.platform||updatePlatform)!==updatePlatform)throw new Error("Plateforme du canal incompatible.");
  if(updateChannel.provider==="github"){
   if(!/^[A-Za-z0-9][A-Za-z0-9-]*\/[A-Za-z0-9_.-]+$/.test(updateChannel.repository)||!["stable","preview"].includes(updateChannel.channel))throw new Error("Dépôt ou canal GitHub invalide.");
   if(!/^[A-Za-z0-9_.-]+\.json$/.test(updateChannel.manifest_asset))throw new Error("Nom du manifeste GitHub invalide.");
  }else if(!updateChannel.provider||updateChannel.provider==="manifest")httpsURL(updateChannel.manifest_url);
  else throw new Error("Fournisseur de mise à jour inconnu.");
  await crypto.subtle.importKey("spki",decode64(updateChannel.public_key_spki),{name:"ECDSA",namedCurve:"P-256"},false,["verify"]);
  updateStatus("idle","Mises à jour signées depuis "+(updateChannel.repository||"le canal configuré")+".");
  const staged=await stagedUpdate();
  if(staged)updateStatus("staged","Version "+staged.version+" prête à installer.",staged);
  if(updateChannel.auto_check!==false&&!staged){
   const automatic=async()=>{
    if(updateBusy||await stagedUpdate())return;
    try{const state=JSON.parse(await IOUtils.readUTF8(PathUtils.join(updateRoot,"last-check.json")));
     if(state.source===JSON.stringify(updateChannel)&&Date.now()-state.time<86400000)return;
    }catch{}
    runUpdate(checkUpdate);
   };
   setTimeout(automatic,10000+Math.random()*20000);setInterval(automatic,3600000);
  }
 }catch(e){updateChannel=null;updateStatus("error",String(e));}
}
async function checkUpdate(){
 verifiedUpdate=null;
 if(!channelConfigured()){updateStatus("unconfigured","Configurer le dépôt et la clé publique dans assets/update-channel.json.");return;}
 const staged=await stagedUpdate();if(staged){updateStatus("staged","Version "+staged.version+" prête à installer.",staged);return;}
 if(Date.now()<updateRetryAt)throw new Error("Vérification suspendue jusqu’à "+new Date(updateRetryAt).toLocaleTimeString()+" (limite GitHub).");
 await IOUtils.makeDirectory(updateRoot,{ignoreExisting:true});
 await IOUtils.writeUTF8(PathUtils.join(updateRoot,"last-check.json"),JSON.stringify({source:JSON.stringify(updateChannel),time:Date.now()}));
 updateStatus("checking","Recherche d’une mise à jour sur "+(updateChannel.repository||"le canal configuré")+"…");
 const {release,envelope}=updateChannel.provider==="github"?await githubRelease():{envelope:await requestUpdateJSON(updateChannel.manifest_url)};
 const payload=decode64(envelope.payload),signature=decode64(envelope.signature);
 const key=await crypto.subtle.importKey("spki",decode64(updateChannel.public_key_spki),{name:"ECDSA",namedCurve:"P-256"},false,["verify"]);
 if(!await crypto.subtle.verify({name:"ECDSA",hash:"SHA-256"},key,signature,payload))throw new Error("Signature de mise à jour invalide.");
 const m=JSON.parse(new TextDecoder().decode(payload));
 if(m.platform!==updatePlatform||!updateVersion.test(m.version)||!/^[a-f0-9]{64}$/.test(m.sha256)||!Number.isSafeInteger(m.size)||m.size<=0||m.size>2147483648)throw new Error("Manifeste invalide.");
 httpsURL(m.url);
 if(release){
  if(release.tag_name.replace(/^v/,"")!==m.version)throw new Error("Version signée différente du tag GitHub.");
  const matches=release.assets.filter(a=>a.state==="uploaded"&&a.size===m.size&&githubAssetURL(release,a)===m.url);
  if(matches.length!==1)throw new Error("Le paquet signé est absent de cette Release GitHub.");
  m.release_url="https://github.com/"+updateChannel.repository+"/releases/tag/"+encodeURIComponent(release.tag_name);
 }
 if(Services.vc.compare(m.version,Services.appinfo.version)<=0){updateStatus("current","CyUrium est à jour ("+Services.appinfo.version+").");return;}
 verifiedUpdate=m;updateStatus("available","Version "+m.version+" disponible.",{version:m.version,release_url:m.release_url||""});
}
function fileHash(file){const stream=Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);stream.init(file,0x01,0,0);
 try{const hash=Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);hash.init(hash.SHA256);hash.updateFromStream(stream,0xffffffff);return [...hash.finish(false)].map(c=>c.charCodeAt(0).toString(16).padStart(2,"0")).join("");}finally{stream.close();}}
async function stageUpdate(){
 if(!verifiedUpdate)throw new Error("Recherchez d’abord une mise à jour signée.");
 await IOUtils.makeDirectory(updateRoot,{ignoreExisting:true});await clearUpdateArchives();
 // Gecko can retain a ZIP mapping after close. Never overwrite that mapping
 // when retrying a failed package in the same engine process.
 const archive=PathUtils.join(updateRoot,"package-"+Services.uuid.generateUUID().toString().replace(/[{}]/g,"")+".zip");
 updateStatus("downloading","Téléchargement de CyUrium "+verifiedUpdate.version+"…",{progress:0});
 const d=await Downloads.createDownload({source:{url:verifiedUpdate.url,adjustChannel:guardUpdateChannel,allowHttpStatus:(_,status)=>status===200},target:archive});
 let oversized=false,lastProgress=-1;
 d.onchange=()=>{
  if(!oversized&&(d.currentBytes>verifiedUpdate.size||d.totalBytes>verifiedUpdate.size)){oversized=true;d.cancel().catch(()=>{});}
  if(d.progress!==lastProgress){lastProgress=d.progress;updateStatus("downloading","Téléchargement : "+d.progress+" %",{progress:d.progress});}
 };
 try{await d.start();}catch(e){if(oversized)throw new Error("Le paquet dépasse la taille signée.");throw e;}
 const info=await IOUtils.stat(archive);if(info.size!==verifiedUpdate.size||fileHash(new FileUtils.File(archive))!==verifiedUpdate.sha256){await IOUtils.remove(archive);throw new Error("Empreinte du paquet incorrecte.");}
 updateStatus("verifying","Signature et empreinte vérifiées. Préparation du paquet…");
 const stage=PathUtils.join(updateRoot,"payload-"+Services.uuid.generateUUID().toString().replace(/[{}]/g,""));await IOUtils.makeDirectory(stage);
 const zip=Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);zip.open(new FileUtils.File(archive));let size=0,count=0;const seen=new Set();
 try{for(const name of zip.findEntries(null)){
   const entry=zip.getEntry(name);if(entry.isDirectory)continue;
   const parts=name.split("/");if(++count>50000||!parts.length||parts.some(p=>!p||p==="."||p===".."||/[\\:\x00-\x1f]/.test(p)||/[. ]$/.test(p)||/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)/i.test(p))||seen.has(name.toLowerCase()))throw new Error("Chemin de paquet invalide.");
   seen.add(name.toLowerCase());size+=entry.realSize;if(size>4*1024*1024*1024)throw new Error("Paquet trop grand.");
   const target=PathUtils.join(stage,...parts);await IOUtils.makeDirectory(PathUtils.parent(target),{ignoreExisting:true});zip.extract(name,new FileUtils.File(target));
 }}finally{zip.close();}
 if(!await IOUtils.exists(PathUtils.join(stage,"CyUrium.exe"))||!await IOUtils.exists(PathUtils.join(stage,"package-files.json")))throw new Error("Paquet CyUrium incomplet.");
 await IOUtils.writeUTF8(PathUtils.join(updateRoot,"staged.json"),JSON.stringify({stage,version:verifiedUpdate.version,sha256:verifiedUpdate.sha256}));
 updateStatus("staged","Version "+verifiedUpdate.version+" vérifiée et prête. L’installation nécessite un redémarrage.",{stage,version:verifiedUpdate.version});
}
function runUpdate(action){
 if(updateBusy)return;updateBusy=true;
 action().catch(e=>updateStatus("error",String(e))).finally(()=>updateBusy=false);
}
async function updateCommand(m){
 if(!["update-check","update-download","update-apply"].includes(m.type))return false;
 if(updateBusy)return true;
 if(m.type==="update-apply"){
  const staged=JSON.parse(await IOUtils.readUTF8(PathUtils.join(updateRoot,"staged.json")));
  if(!updateVersion.test(staged.version)||Services.vc.compare(staged.version,Services.appinfo.version)<=0)throw new Error("Aucune nouvelle version prête à installer.");
  send({type:"update-ready",stage:staged.stage});return true;
 }
 // Network and disk operations run asynchronously; page commands keep flowing.
 runUpdate(m.type==="update-check"?checkUpdate:stageUpdate);return true;
}
