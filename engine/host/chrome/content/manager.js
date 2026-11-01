// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
"use strict";
const tabs=new Map();
let bridgeSecret=Services.env.get("CYURIUM_GECKO_TOKEN");
const windowId=Services.env.get("CYURIUM_GECKO_WINDOW_ID");
let output,transport,pump,buffer="",queue=[],processing=Promise.resolve(),closing=false,authenticated=false,bridgeKey,bridgeTranscript,authTimer,queuedCommands=0;
const decoder=new TextDecoder();const encoder=new TextEncoder();
function send(message){
  if(closing)return;
  const bytes=encoder.encode(JSON.stringify(message)+"\n");
  if(bytes.length>8*1024*1024||queue.length>=2048||queue.reduce((n,s)=>n+s.length,0)+bytes.length>16*1024*1024)throw new Error("Bridge output limit");
  let data="";for(let i=0;i<bytes.length;i+=16384)data+=String.fromCharCode(...bytes.subarray(i,i+16384));queue.push(data);flush();
}
function flush(){
  if(!output)return;
  try {while(queue.length){const data=queue[0],written=output.write(data,data.length);if(written<data.length){queue[0]=data.slice(written);break;}queue.shift();}}
  catch(e){if(e.result!==Cr.NS_BASE_STREAM_WOULD_BLOCK)console.error(e);}
  if(queue.length)output.QueryInterface(Ci.nsIAsyncOutputStream).asyncWait({onOutputStreamReady:flush},0,0,Services.tm.currentThread);
}
function state(tab){
  if(!tab.browser.isConnected)return;
  const b=tab.browser;
  send({type:"state",tab:tab.id,url:b.currentURI?.spec||"",title:tab.title||b.contentTitle||"",
    loading:tab.loading,canBack:b.canGoBackIgnoringUserInteraction,canForward:b.canGoForward,error:tab.error||"",
    security:tab.security||"unknown",securityOrigin:tab.securityOrigin||"",securityIssuer:tab.securityIssuer||"",
    request:tab.request||0,operation:tab.operation||"load"});
}
async function createTab(id){
  if(tabs.has(id))return tabs.get(id);
  return new Promise((resolve,reject)=>{
    const options={attach(win){
      try {
      win.document.title="CyUrium Gecko "+windowId+" "+id;
      const b=win.document.getElementById("web");
      const tab={id,window:win,browser:b,loading:false,title:"",error:"",request:0,operation:"load"};tabs.set(id,tab);
      installExtensionHost(tab);
      b.addEventListener("oop-browser-crashed",()=>{cancelTabPrompts(id);tabs.delete(id);win.close();send({type:"tab-crashed",tab:id});});
      win.CyUriumTab={document(data){if(data.url===b.currentURI?.spec){tab.title=data.title;state(tab);
        if(data.ready&&tab.restore?.url===data.url){const saved=tab.restore;tab.restore=null;b.browsingContext.currentWindowGlobal.getActor("CyUriumContent").sendQuery("restore-state",saved).catch(console.error);}
      }},session(data){if(data.url===b.currentURI?.spec)send({type:"session-state",tab:id,data});}};
      const listener={
        QueryInterface:ChromeUtils.generateQI(["nsIWebProgressListener","nsISupportsWeakReference"]),
        onStateChange(progress,request,flags,status){
          if(!progress.isTopLevel||!(flags&Ci.nsIWebProgressListener.STATE_IS_NETWORK))return;
          if(flags&Ci.nsIWebProgressListener.STATE_START){tab.loading=true;tab.error="";}
          if(flags&Ci.nsIWebProgressListener.STATE_STOP){
            tab.loading=false;
            if(!Components.isSuccessCode(status)&&status!==Cr.NS_BINDING_ABORTED){
              const blocked=["NS_ERROR_PHISHING_URI","NS_ERROR_MALWARE_URI","NS_ERROR_UNWANTED_URI","NS_ERROR_HARMFUL_URI"].some(name=>Cr[name]!==undefined&&(Cr[name]>>>0)===(status>>>0));
              tab.error=blocked?"Page bloquée par la protection contre les sites trompeurs ou dangereux.":(status>>>16)===0x805a?"Connexion sécurisée refusée par Gecko : certificat ou protocole non validé.":"Le chargement a échoué (0x"+(status>>>0).toString(16)+").";
            }
          }
          if(flags&Ci.nsIWebProgressListener.STATE_STOP)updateTabSecurity(tab);else state(tab);
          if(flags&Ci.nsIWebProgressListener.STATE_STOP)tab.request=0;
        },
        onLocationChange(progress){if(progress.isTopLevel){cancelTabPrompts(id);tab.title="";tab.security="unknown";tab.securityOrigin="";state(tab);}},
        onProgressChange(){},onStatusChange(){},onSecurityChange(progress){if(progress.isTopLevel)updateTabSecurity(tab);},onContentBlockingEvent(){}
      };
      tab.listener=listener;b.addProgressListener(listener,Ci.nsIWebProgress.NOTIFY_ALL);
      const domWindow={
        QueryInterface:ChromeUtils.generateQI(["nsIBrowserDOMWindow"]),
        openURI(uri,params,where,flags,triggeringPrincipal){if(uri)send({type:"new-tab",opener:id,url:uri.spec});return null;},
        createContentWindow(){return null;},openURIInFrame(params,where,flags){if(params?.uri)send({type:"new-tab",opener:id,url:params.uri.spec});return null;},
        createContentWindowInFrame(){return null;},canClose(){return true;},get tabCount(){return tabs.size;}
      };
      win.browserDOMWindow=domWindow;
      const keys=win.document.createXULElement("keyset");win.document.documentElement.appendChild(keys);
      for(const [key,modifiers,keycode] of [
        ["l","accel"],["t","accel"],["n","accel"],["w","accel"],["w","accel,shift"],["q","accel"],
        ["r","accel"],["h","accel"],["j","accel"],["d","accel"],[",","accel"],
        ["tab","accel","VK_TAB"],["tab","accel,shift","VK_TAB"],["left","alt","VK_LEFT"],["right","alt","VK_RIGHT"],
        ["f5","","VK_F5"],["f6","","VK_F6"],["f11","","VK_F11"],
        ...Array.from({length:9},(_,i)=>[String(i+1),"accel"])
      ]){
        const element=win.document.createXULElement("key");element.setAttribute(keycode?"keycode":"key",keycode||key);
        element.setAttribute("modifiers",modifiers);element.setAttribute("reserved","true");
        element.addEventListener("command",event=>{event.preventDefault();send({type:"shortcut",tab:id,key,shift:modifiers.includes("shift"),alt:modifiers.includes("alt"),control:modifiers.includes("accel")});});keys.appendChild(element);
      }
      send({type:"tab-ready",tab:id,title:win.document.title});state(tab);resolve(tab);
      }catch(e){tabs.delete(id);win.close();reject(e);}
    }};
    window.openDialog("chrome://cyurium/content/tab.xhtml","_blank","chrome,dialog=no,titlebar=no,border=no,resizable=no,left=-10000,top=-10000",{wrappedJSObject:options});
  });
}
async function command(message){
  if(message.type==="session-policy"){
    Services.prefs.setBoolPref("cyurium.session.restore_forms",message.restoreForms===true);
    for(const tab of tabs.values())try{tab.browser.browsingContext.currentWindowGlobal.getActor("CyUriumContent").sendQuery("capture-state").catch(()=>{});}catch{}
    return;
  }
  if(message.type==="workspace-layout"){workspaceLayout=message;applyExtensionLayout();return;}
  if(message.type==="service-qa"&&Services.env.get("CYURIUM_GECKO_TEST")==="1"){
    if(message.action==="security"){publish("security",securitySnapshot());return;}
    if(message.action==="panels"){
      let panels=[];for(let i=0;i<50;++i){panels=[...Services.wm.getEnumerator("cyurium:extension-panel")];if(panels.some(w=>w.document.getElementById("extension-panel")?.currentURI?.scheme==="moz-extension"))break;await new Promise(resolve=>setTimeout(resolve,100));}
      publish("panels",panels.map(w=>({url:w.document.getElementById("extension-panel")?.currentURI?.spec,type:w.document.getElementById("extension-panel")?.getAttribute("webextension-view-type")})));return;
    }
    if(message.action==="close-panels"){for(const panel of Services.wm.getEnumerator("cyurium:extension-panel"))panel.close();return;}
    if(message.action==="extension-runtime"){
      const {ExtensionParent}=ChromeUtils.importESModule("resource://gre/modules/ExtensionParent.sys.mjs");
      let extension;for(let i=0;i<50;++i){extension=ExtensionParent.GlobalManager.getExtension(message.id);if(extension?.tabManager)break;await new Promise(resolve=>setTimeout(resolve,100));}
      publish("extension-runtime",{background:extension?.backgroundState,windows:[...ExtensionParent.apiManager.global.windowTracker.browserWindows()].map(w=>({id:w.cyuriumWindowId,count:w.gBrowser.tabs.length})),tabs:extension?[...extension.tabManager.query()].map(t=>t.convert()):[]});return;
    }
    if(message.action==="show"){const tab=tabs.get(message.tab);tab.window.moveTo(50,50);tab.window.resizeTo(780,560);tab.browser.docShellIsActive=true;tab.browser.renderLayers=true;return;}
    if(message.action==="exercise")await tabs.get(message.tab).browser.browsingContext.currentWindowGlobal.getActor("CyUriumContent").sendQuery("exercise",{action:message.value});
    return;
  }
  if(await serviceCommand(message))return;
  if(await updateCommand(message))return;
  if(message.type==="shutdown"){
    closing=true;for(const tab of tabs.values())tab.window.close();tabs.clear();
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);return;
  }
  if(message.type==="close"){cancelTabPrompts(message.tab);const tab=tabs.get(message.tab);if(tab){tab.window.close();tabs.delete(message.tab);}return;}
  if(message.type==="create"){await createTab(message.tab);return;}
  const tab=tabs.get(message.tab);if(!tab)return;
  if(message.type==="automation"){
    try {
      const parameters=message.parameters;
      if(!["page_snapshot","page_click","page_fill","page_scroll"].includes(message.operation))throw new Error("Unknown page operation");
      if(!["http","https"].includes(tab.browser.currentURI.scheme)||tab.browser.currentURI.spec!==parameters.url)throw new Error("Page URL changed");
      const actor=tab.browser.browsingContext.currentWindowGlobal.getActor("CyUriumContent");
      const data=await actor.sendQuery("automation",{operation:message.operation,parameters});
      send({type:"automation-result",tab:tab.id,request:message.request,data});
    }catch(error){send({type:"automation-result",tab:tab.id,request:message.request,error:String(error)});}
    return;
  }
  if(message.type==="restore"){tab.restore=message.data;return;}
  if(message.type==="active"){tab.browser.docShellIsActive=Boolean(message.value);tab.browser.renderLayers=Boolean(message.value);return;}
  if(message.type==="navigate"){
    tab.request=message.sequence;tab.operation=message.action;tab.error="";
    if(message.action==="load"){
      const uri=Services.io.newURI(message.url);
      const extension=uri.scheme==="moz-extension"?WebExtensionPolicy.getByURI(uri):null;
      if(!["http","https","file"].includes(uri.scheme)&&!extension){tab.error="Ce protocole n'est pas autorisé dans une page web.";state(tab);return;}
      tab.browser.loadURI(uri,{triggeringPrincipal:extension?extension.extension.principal:Services.scriptSecurityManager.createNullPrincipal({}),hasValidUserGestureActivation:true});
    }else if(message.action==="back"){if(tab.browser.canGoBackIgnoringUserInteraction)tab.browser.goBack(false);}
    else if(message.action==="forward"){if(tab.browser.canGoForward)tab.browser.goForward(false);}
    else if(message.action==="reload")tab.browser.reload();
    else if(message.action==="stop"){tab.browser.stop();tab.loading=false;state(tab);}
  }
  if(message.type==="inspect"&&Services.env.get("CYURIUM_GECKO_TEST")==="1"){
    try{
      const data=await tab.browser.browsingContext.currentWindowGlobal.getActor("CyUriumContent").sendQuery("inspect",{});
      if(message.request===6){
        const b=tab.browser,width=b.clientWidth,height=b.clientHeight;
        const bitmap=await b.browsingContext.currentWindowGlobal.drawSnapshot(new DOMRect(0,0,width,height),1,"white");
        const canvas=document.createElementNS("http://www.w3.org/1999/xhtml","canvas");canvas.width=width;canvas.height=height;
        canvas.getContext("2d").drawImage(bitmap,0,0);bitmap.close();
        const blob=await new Promise(resolve=>canvas.toBlob(resolve,"image/png"));
        await IOUtils.write(PathUtils.join(Services.env.get("MOZ_APP_DATA"),"web-content.png"),new Uint8Array(await blob.arrayBuffer()));
      }
      send({type:"inspection",tab:tab.id,request:message.request,data});
    }catch(e){send({type:"inspection",tab:tab.id,request:message.request,error:String(e)});}
  }
}
function bridgeHex(bytes){return Array.from(bytes,c=>c.toString(16).padStart(2,"0")).join("");}
async function bridgeMessage(message){
  if(closing)return;
  if(authenticated){if(["challenge","authenticated"].includes(message.type))throw new Error("Unexpected authentication frame");return command(message);}
  if(message.type==="challenge"&&!bridgeTranscript){
    if(message.protocol!==1||!/^[a-f0-9]{64}$/.test(message.nonce)||!/^[a-f0-9]{64}$/.test(bridgeSecret))throw new Error("Invalid bridge challenge");
    const hello={type:"hello",protocol:1,nonce:bridgeHex(crypto.getRandomValues(new Uint8Array(32))),pid:Services.appinfo.processID,version:Services.appinfo.platformVersion,remote:Services.appinfo.browserTabsRemoteAutostart,fission:Services.appinfo.fissionAutostart};
    bridgeTranscript="cyurium-bridge-v1\n"+message.nonce+"\n"+hello.nonce+"\n"+hello.pid+"\n"+hello.version+"\n"+(hello.remote?"1":"0")+"\n"+(hello.fission?"1":"0");
    bridgeKey=await crypto.subtle.importKey("raw",encoder.encode(bridgeSecret),{name:"HMAC",hash:"SHA-256"},false,["sign","verify"]);
    bridgeSecret="";Services.env.set("CYURIUM_GECKO_TOKEN","");
    hello.proof=bridgeHex(new Uint8Array(await crypto.subtle.sign("HMAC",bridgeKey,encoder.encode("client\n"+bridgeTranscript))));send(hello);return;
  }
  if(message.type!=="authenticated"||message.protocol!==1||!bridgeTranscript||!/^[a-f0-9]{64}$/.test(message.proof))throw new Error("Unauthenticated native bridge");
  const proof=Uint8Array.from(message.proof.match(/../g),c=>parseInt(c,16));
  if(!await crypto.subtle.verify("HMAC",bridgeKey,proof,encoder.encode("server\n"+bridgeTranscript)))throw new Error("Native bridge authentication failed");
  authenticated=true;bridgeKey=null;bridgeTranscript="";clearTimeout(authTimer);
  await initServices();
}
function bridgeFailure(error){
  if(authenticated&&!error.fatalSafety){send({type:"error",message:String(error),stack:error.stack});return;}
  terminateBridge();
}
function terminateBridge(){
  if(closing)return;closing=true;clearTimeout(authTimer);bridgeKey=null;bridgeSecret="";
  try{pump?.cancel(Cr.NS_BINDING_ABORTED);}catch{}try{output?.close();}catch{}try{transport?.close(Cr.NS_ERROR_ABORT);}catch{}
  for(const tab of tabs.values())try{tab.window.close();}catch{}tabs.clear();
  Services.tm.dispatchToMainThread(()=>{try{window.close();}catch{}Services.startup.quit(Ci.nsIAppStartup.eForceQuit);});
}
function connect(){
  if(!/^[a-f0-9]{32}$/.test(windowId))throw new Error("Invalid native window identifier");
  ChromeUtils.registerWindowActor("CyUriumContent",{
    parent:{esModuleURI:"chrome://cyurium/content/ContentParent.sys.mjs"},
    child:{esModuleURI:"chrome://cyurium/content/ContentChild.sys.mjs",events:{DOMContentLoaded:{},pageshow:{},DOMTitleChanged:{},input:{capture:true},change:{capture:true},scroll:{capture:true}}},
    allFrames:false,includeChrome:false,messageManagerGroups:["cyurium"],matches:["http://*/*","https://*/*","file:///*"]
  });
  document.title="CyUrium Gecko Manager "+windowId;
  const service=Cc["@mozilla.org/network/socket-transport-service;1"].getService(Ci.nsISocketTransportService);
  transport=service.createTransport([],"127.0.0.1",Number(Services.env.get("CYURIUM_GECKO_PORT")),null,null);
  output=transport.openOutputStream(0,0,0);
  const input=transport.openInputStream(0,0,0);
  pump=Cc["@mozilla.org/network/input-stream-pump;1"].createInstance(Ci.nsIInputStreamPump);pump.init(input,0,0,true);
  pump.asyncRead({
    onStartRequest(){},
    onDataAvailable(request,stream,offset,count){
      const reader=Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);reader.setInputStream(stream);
      buffer+=decoder.decode(new Uint8Array(reader.readByteArray(count)),{stream:true});
      if(buffer.length>8*1024*1024)throw new Error("Oversized bridge command");
      let end;while((end=buffer.indexOf("\n"))>=0){const line=buffer.slice(0,end);buffer=buffer.slice(end+1);
        if(++queuedCommands>2048){bridgeFailure(new Error("Bridge command limit"));return;}
        processing=processing.then(()=>bridgeMessage(JSON.parse(line))).catch(bridgeFailure).finally(()=>--queuedCommands);
      }
    },
    onStopRequest(){terminateBridge();}
  });
  authTimer=setTimeout(()=>{if(!authenticated)bridgeFailure(new Error("Bridge authentication timed out"));},5000);
}
window.addEventListener("DOMContentLoaded",()=>{try{connect();}catch(e){
  dump("CyUrium bridge startup: "+e+"\n"+e.stack+"\n");
  IOUtils.writeUTF8(PathUtils.join(Services.env.get("MOZ_APP_DATA"),"startup-error.txt"),String(e)+"\n"+e.stack)
    .finally(()=>Services.startup.quit(Ci.nsIAppStartup.eForceQuit));
}},{once:true});
