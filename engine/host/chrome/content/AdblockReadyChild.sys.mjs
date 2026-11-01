// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
// Read-only startup handshake with the unchanged uBlock dashboard. Its own
// script removes notReady only after its readyToFilter message returns true.
export class CyUriumAdblockReadyChild extends JSWindowActorChild {
 receiveMessage(message){
  if(message.name!=="ready"||this.document.nodePrincipal.addonId!=="uBlock0@raymondhill.net"||
     this.document.documentURIObject.filePath!=="/dashboard.html")return false;
  return this.document.readyState!=="loading"&&!!this.document.body&&!this.document.body.classList.contains("notReady");
 }
}
