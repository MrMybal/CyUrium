// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
"use strict";
async function ensureBundledExtensions(){
 const folder=PathUtils.join(Services.dirsvc.get("GreD",Ci.nsIFile).path,"app","extensions");
 const lock=await IOUtils.readJSON(PathUtils.join(folder,"bundled-extensions.json"));
 if(lock.schema!==1||!Array.isArray(lock.extensions))throw new Error("Catalogue des extensions intégrées invalide.");
 for(const entry of lock.extensions){
  if(entry.id!=="uBlock0@raymondhill.net"||!/^[-A-Za-z0-9_.]+\.xpi$/.test(entry.file)||!/^[a-f0-9]{64}$/.test(entry.sha256))throw new Error("Extension intégrée inconnue.");
  const marker="cyurium.extensions.defaults."+entry.id;
  // This is an initial default, never a policy that reinstalls or re-enables
  // something the user has removed/disabled. Never downgrade an existing copy.
  if(!Services.prefs.getBoolPref(marker,false)){
   if(!await AddonManager.getAddonByID(entry.id)){
    const path=PathUtils.join(folder,entry.file),bytes=await IOUtils.read(path);
    const hash=Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256",bytes)),b=>b.toString(16).padStart(2,"0")).join("");
    if(hash!==entry.sha256)throw new Error("Le paquet uBlock Origin a été modifié : installation refusée.");
    const install=await AddonManager.getInstallForFile(new FileUtils.File(path));
    const addon=install.addon;
    if(install.error||!addon||addon.id!==entry.id||addon.version!==entry.version||addon.appDisabled||addon.signedState<=AddonManager.SIGNEDSTATE_MISSING){
     install.cancel();throw new Error("Le paquet uBlock Origin n’est pas signé ou compatible.");
    }
    // Only this pinned, signed default is pre-approved. User-selected XPIs still
    // go through installExtension's native permission confirmation.
    await install.install();
    if(!(await AddonManager.getAddonByID(entry.id))?.isActive)throw new Error("uBlock Origin n’a pas pu être activé.");
   }
   Services.prefs.setBoolPref(marker,true);
   Services.prefs.savePrefFile(null);
  }
  if((await AddonManager.getAddonByID(entry.id))?.isActive){
   // Installation/AddonManager readiness precedes background script readiness.
   // Wait for module evaluation and listener registration before opening the
   // first page, including on subsequent launches of an existing profile.
   const policy=WebExtensionPolicy.getByID(entry.id),extension=policy?.extension;
   let timer;
   try{
    await Promise.race([extension?.promiseBackgroundStarted?.()||Promise.reject(new Error("Démarrage de uBlock Origin indisponible.")),
     new Promise((_,reject)=>{timer=setTimeout(()=>reject(new Error("Délai de démarrage de uBlock Origin dépassé.")),20000);})]);
    if(extension.backgroundState!=="running")throw new Error("uBlock Origin n’a pas démarré.");
   }finally{clearTimeout(timer);}
   await waitForAdblockFilters(extension);
  }
 }
}
async function waitForAdblockFilters(extension){
 // uBO initializes its filters asynchronously even after its background page
 // is loaded. Use the existing dashboard's read-only readiness handshake,
 // without modifying the signed XPI, injecting scripts or evaluating code.
 ChromeUtils.registerWindowActor("CyUriumAdblockReady",{child:{esModuleURI:"chrome://cyurium/content/AdblockReadyChild.sys.mjs"},
  allFrames:false,includeChrome:false,messageManagerGroups:["webext-browsers"],matches:["moz-extension://*/*"]});
 const browser=document.createXULElement("browser");
 for(const [name,value] of Object.entries({type:"content",remote:"true",remoteType:"extension",maychangeremoteness:"true",
  messagemanagergroup:"webext-browsers","webextension-view-type":"popup",hidden:"true"}))browser.setAttribute(name,value);
 document.documentElement.appendChild(browser);
 let stopped=false,timer;
 try{
  const {ExtensionParent}=ChromeUtils.importESModule("resource://gre/modules/ExtensionParent.sys.mjs");
  ExtensionParent.apiManager.emit("extension-browser-inserted",browser,{});
  browser.loadURI(Services.io.newURI(extension.getURL("dashboard.html")),{triggeringPrincipal:extension.principal});
  const check=async()=>{while(!stopped){
    try{if(await browser.browsingContext?.currentWindowGlobal?.getActor("CyUriumAdblockReady").sendQuery("ready",{}))return;}catch{}
    if(!stopped)await new Promise(resolve=>setTimeout(resolve,50));
  }};
  await Promise.race([check(),new Promise((_,reject)=>{timer=setTimeout(()=>reject(new Error("Les filtres de uBlock Origin n’ont pas terminé leur chargement.")),20000);})]);
 }finally{stopped=true;clearTimeout(timer);browser.remove();ChromeUtils.unregisterWindowActor("CyUriumAdblockReady");}
}
