// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
export class CyUriumContentParent extends JSWindowActorParent {
  receiveMessage(message) {
    const browser=this.browsingContext.top.embedderElement;
    const win=browser?.ownerDocument?.defaultView;
    if(message.name==="document"&&win?.CyUriumTab)win.CyUriumTab.document(message.data);
    if(message.name==="session"&&win?.CyUriumTab)win.CyUriumTab.session(message.data);
  }
}
