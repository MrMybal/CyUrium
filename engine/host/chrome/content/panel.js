// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
window.addEventListener("DOMContentLoaded",()=>{
 const options=window.arguments[0].wrappedJSObject,browser=document.getElementById("extension-panel");
 document.title=options.title+" — CyUrium";
 const {ExtensionParent}=ChromeUtils.importESModule("resource://gre/modules/ExtensionParent.sys.mjs");
 ExtensionParent.apiManager.emit("extension-browser-inserted",browser,{parentWindowId:options.windowId});
 browser.loadURI(Services.io.newURI(options.url),{triggeringPrincipal:options.principal});
},{once:true});
