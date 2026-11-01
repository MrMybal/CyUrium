// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
export class WebRTCParent extends JSWindowActorParent {
  receiveMessage(message){
    const browser=this.browsingContext.top.embedderElement;
    const host=Services.wm.getEnumerator("");let manager;
    for(const win of host)if(typeof win.CyUriumMediaRequest==="function"){manager=win;break;}
    if(message.name==="rtcpeer:Request")this.sendAsyncMessage("rtcpeer:Allow",message.data);
    else if(message.name==="webrtc:Request"){
      if(manager)manager.CyUriumMediaRequest(this,browser,message.data,this.manager.documentPrincipal);
      else this.sendAsyncMessage("webrtc:Deny",message.data);
    }else if(message.name==="webrtc:CancelRequest")manager?.CyUriumMediaCancel(message.data);
    else if(message.name==="webrtc:UpdateIndicators")manager?.CyUriumMediaIndicator(browser,{...message.data,windowID:message.data.windowId,origin:this.manager.documentPrincipal.origin},this);
  }
}
