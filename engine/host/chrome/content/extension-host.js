// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
"use strict";
// Small tabbrowser adapter for Gecko's WebExtension APIs. It exposes our real
// browser elements, while the native workspace remains the owner of the tabs.
let workspaceLayout={windows:[]};
function installExtensionHost(tab){
 const win=tab.window,b=tab.browser;
 const container=win.document.createXULElement("tabs");container.hidden=true;win.document.documentElement.appendChild(container);
 const native=win.document.createXULElement("tab");container.appendChild(native);
 Object.assign(native,{linkedBrowser:b,linkedPanel:"web",_tPos:0,lastAccessed:Date.now(),userContextId:0,muted:false,pinned:false});tab.extensionTab=native;
 win.gBrowserInit={delayedStartupFinished:true,isAdoptingTab:()=>false,getTabToAdopt:()=>null};win.delayedStartupPromise=Promise.resolve();
 const progress=new Map();
 const facade={tabContainer:container,selectedTab:native,selectedBrowser:b,tabs:[native],visibleTabs:[native],selectedTabs:[native],browsers:[b],
  getTabForBrowser(browser){return [...tabs.values()].find(t=>t.browser===browser)?.extensionTab;},
  getIcon(){return null;},getTabSharingState(native){const item=[...tabs.values()].find(t=>t.extensionTab===native),media=[...mediaIndicators.values()].filter(m=>m.tab===item?.id);return {camera:media.some(m=>!!m.camera),microphone:media.some(m=>!!m.microphone),screen:media.find(m=>m.screen)?.screen||undefined};},
  getBrowserForTab(t){return t.linkedBrowser;},
  addTabsProgressListener(listener){const bridge={QueryInterface:ChromeUtils.generateQI(["nsIWebProgressListener","nsISupportsWeakReference"])};
   for(const name of ["onStateChange","onLocationChange","onSecurityChange","onProgressChange","onStatusChange","onContentBlockingEvent"])bridge[name]=(...args)=>listener[name]?.(b,...args);
   progress.set(listener,bridge);b.addProgressListener(bridge,Ci.nsIWebProgress.NOTIFY_ALL);},
  removeTabsProgressListener(listener){const bridge=progress.get(listener);if(bridge){b.removeProgressListener(bridge);progress.delete(listener);}},
  getTabFromBrowser(browser){return this.getTabForBrowser(browser);},
  removeTab(t){send({type:"extension-tab-close",tab:[...tabs.values()].find(v=>v.extensionTab===t)?.id||0});},
  pinTab(){throw new Error("Les onglets épinglés ne sont pas encore disponibles dans CyUrium.");},
  unpinTab(){},isFindBarInitialized:()=>false,
  get selectedBrowsers(){return [this.selectedBrowser];},
 };
 win.gBrowser=facade;
 win.SidebarController={promiseInitialized:Promise.resolve(),currentID:null};
 for(const id of ["placesContext","menu_ToolsPopup","tabContextMenu","sidebar-bookmarks-context-menu","sidebar-switcher-target"]){const node=win.document.createXULElement("menupopup");node.id=id;win.document.documentElement.appendChild(node);}
 win.document.documentElement.setAttribute("windowtype","navigator:browser");
 win.gBrowserBundle=Services.strings.createBundle("chrome://browser/locale/browser.properties");
 applyExtensionLayout();
}
function applyExtensionLayout(){
 for(const tab of tabs.values()){
  if(!tab.extensionTab)continue;
  const group=workspaceLayout.windows.find(w=>w.tabs.includes(tab.id))||{id:tab.id,selected:tab.id,tabs:[tab.id]};
  const members=group.tabs.map(id=>tabs.get(id)).filter(t=>t?.extensionTab),active=tabs.get(group.selected)||tab;
  tab.window.cyuriumWindowId=group.id;
  Object.assign(tab.window.gBrowser,{tabs:members.map(t=>t.extensionTab),visibleTabs:members.map(t=>t.extensionTab),browsers:members.map(t=>t.browser),selectedTab:active.extensionTab||tab.extensionTab,selectedBrowser:active.browser,selectedTabs:[active.extensionTab||tab.extensionTab]});
  tab.extensionTab.setAttribute("selected",tab.id===group.selected?"true":"false");tab.extensionTab._tPos=group.tabs.indexOf(tab.id);
 }
}
window.CyUriumExtensionHost={get tabs(){return tabs;},get layout(){return workspaceLayout;}};
