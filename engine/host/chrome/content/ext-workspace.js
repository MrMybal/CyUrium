// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
"use strict";
// Executed inside Gecko's shared extension API scope after ext-browser.js.
Promise.resolve().then(()=>{
 if(global.cyuriumWorkspaceReady)return;global.cyuriumWorkspaceReady=true;
 const host=()=>[...Services.wm.getEnumerator("")].map(win=>win.wrappedJSObject||win).find(win=>win.CyUriumExtensionHost)?.CyUriumExtensionHost;
 const originalGetId=windowTracker.getId.bind(windowTracker),originalGetWindow=windowTracker.getWindow.bind(windowTracker);
 windowTracker.getId=win=>{
  const h=host();if(h)for(const tab of h.tabs.values())if(tab.window.browsingContext.id===win?.browsingContext?.id)return h.layout.windows.find(w=>w.tabs.includes(tab.id))?.id||tab.id;
  return originalGetId(win);
 };
 windowTracker.getWindow=(id,context,...options)=>{
  const h=host();if(h)for(const tab of h.tabs.values())if(tab.window.cyuriumWindowId===id)return tab.window;
  return originalGetWindow(id,context,...options);
 };
 windowTracker.browserWindows=function*(){const h=host(),seen=new Set();if(!h)return;
  for(const tab of h.tabs.values())if(tab.extensionTab&&!seen.has(tab.window.cyuriumWindowId)){seen.add(tab.window.cyuriumWindowId);yield tab.window;}
 };
 Object.defineProperty(Tab.prototype,"windowId",{get(){const h=host();if(h)for(const tab of h.tabs.values())if(tab.browser.browserId===this.browser.browserId)return h.layout.windows.find(w=>w.tabs.includes(tab.id))?.id||tab.id;return windowTracker.getId(this.window);}});
 Object.defineProperty(windowTracker,"topWindow",{get(){const h=host();if(!h)return null;
  const group=h.layout.windows.find(w=>w.id===h.layout.focused)||h.layout.windows[0];return h.tabs.get(group?.selected)?.window||[...h.tabs.values()][0]?.window||null;}});
 Object.defineProperty(windowTracker,"topNonPBWindow",{get(){
  const current=this.topWindow;if(current&&!PrivateBrowsingUtils.isWindowPrivate(current))return current;
  return [...this.browserWindows()].find(win=>!PrivateBrowsingUtils.isWindowPrivate(win))||null;}});
});
