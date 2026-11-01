// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
"use strict";
let safeBrowsing;
function updateTabSecurity(tab){
 const browser=tab.browser,flags=browser.securityUI?.state||0;
 let origin="";try{origin=new URL(browser.currentURI.spec).origin;}catch{}
 tab.securityOrigin=origin==="null"?"":origin;
 tab.security="unknown";tab.securityIssuer="";
 if(browser.currentURI?.scheme==="https"){
  if(flags&Ci.nsIWebProgressListener.STATE_IS_SECURE)tab.security="encrypted";
  if(flags&Ci.nsIWebProgressListener.STATE_IS_BROKEN||tab.error)tab.security="broken";
  try{const cert=browser.securityUI.secInfo?.serverCert;tab.securityIssuer=cert?.issuerCommonName||cert?.issuerOrganization||"";}catch{}
 }else if(browser.currentURI?.scheme==="http")tab.security="insecure";
 else if(browser.currentURI?.scheme==="file")tab.security="local";
 state(tab);
}
function securitySnapshot(){
 let effectiveLevel=0,win32k="";
 try{const settings=Cc["@mozilla.org/sandbox/sandbox-settings;1"].getService(Ci.mozISandboxSettings);effectiveLevel=settings.effectiveContentSandboxLevel;win32k=settings.contentWin32kLockdownStateString;}catch{}
 const frames=context=>({url:context.currentWindowGlobal?.documentURI?.spec||"",process:context.currentWindowGlobal?.osPid||0,children:[...context.children].map(frames)});
 return {remote:Services.appinfo.browserTabsRemoteAutostart,fission:Services.appinfo.fissionAutostart,sandboxLevel:effectiveLevel,win32k,
  safeBrowsing:{initialized:!!safeBrowsing?.initialized,phishing:Services.prefs.getBoolPref("browser.safebrowsing.phishing.enabled",false),malware:Services.prefs.getBoolPref("browser.safebrowsing.malware.enabled",false),downloads:Services.prefs.getBoolPref("browser.safebrowsing.downloads.enabled",false)},
  tabs:[...tabs.values()].map(tab=>({id:tab.id,remoteType:tab.browser.remoteType,remoteSubframes:tab.browser.browsingContext.useRemoteSubframes,documentURI:tab.browser.browsingContext.currentWindowGlobal?.documentURI?.spec||"",security:tab.security||"unknown",origin:tab.securityOrigin||"",frames:frames(tab.browser.browsingContext)}))};
}
function initSecurity(){
 const report=securitySnapshot();
 if(!report.remote||!report.fission||report.sandboxLevel<=0){const error=new Error("Le confinement du moteur et l’isolation des sites doivent être actifs.");error.fatalSafety=true;throw error;}
 safeBrowsing=ChromeUtils.importESModule("resource://gre/modules/SafeBrowsing.sys.mjs").SafeBrowsing;safeBrowsing.init();
 publish("security",securitySnapshot());
}
