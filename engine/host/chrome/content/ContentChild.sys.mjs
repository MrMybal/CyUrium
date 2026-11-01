// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
export class CyUriumContentChild extends JSWindowActorChild {
  handleEvent(event) {
    if(["DOMContentLoaded","pageshow","DOMTitleChanged"].includes(event.type))this.sendAsyncMessage("document", {url:this.document.location.href,title:this.document.title,ready:event.type!=="DOMTitleChanged"});
    if(!this.saveTimer)this.saveTimer=this.contentWindow.setTimeout(()=>{this.saveTimer=0;this.saveState();},700);
  }
  saveState(){
    const fields=[];let size=0;
    for(const element of Services.prefs.getBoolPref('cyurium.session.restore_forms',false)?this.document.querySelectorAll('input,textarea,select'):[]){
      const type=(element.type||'').toLowerCase(),autocomplete=element.autocomplete||'';
      if(!element.id&&!element.name||!['text','search','textarea','select-one','select-multiple','checkbox','radio'].includes(type)||element.form?.autocomplete==='off'||/(?:cc-|one-time-code|off|password)/i.test(autocomplete)||/(?:passw|passwd|token|secret|otp|card|iban|credit|cvv|cvc|ssn)/i.test(element.id+' '+element.name))continue;
      const value=element.value;if(value.length>8192||(size+=value.length)>65536||fields.length>=100)break;
      fields.push({id:element.id,name:element.name,type,value,checked:!!element.checked});
    }
    this.sendAsyncMessage('session',{url:this.document.location.href,scroll:[this.contentWindow.scrollX,this.contentWindow.scrollY],fields});
  }
  receiveMessage(message) {
    if(message.name==='automation')return this.automate(message.data);
    if(message.name==='capture-state'){this.saveState();return true;}
    if(message.name==='restore-state'){
      const state=message.data;if(state.url!==this.document.location.href)return false;
      for(const field of Services.prefs.getBoolPref('cyurium.session.restore_forms',false)&&Array.isArray(state.fields)?state.fields.slice(0,100):[]){
        const element=field.id?this.document.getElementById(field.id):this.document.querySelector('[name="'+this.contentWindow.CSS.escape(field.name)+'"]');
        if(!element||element.type!==field.type||!['text','search','textarea','select-one','select-multiple','checkbox','radio'].includes(element.type)||element.form?.autocomplete==='off'||/(?:cc-|one-time-code|off|password)/i.test(element.autocomplete||'')||/(?:passw|passwd|token|secret|otp|card|iban|credit|cvv|cvc|ssn)/i.test(element.id+' '+element.name))continue;
        element.value=String(field.value).slice(0,8192);if(['checkbox','radio'].includes(element.type))element.checked=!!field.checked;
      }
      if(Array.isArray(state.scroll)&&state.scroll.length===2)this.contentWindow.scrollTo(...state.scroll);return true;
    }
    if(!["inspect","exercise"].includes(message.name))return null;
    // Read-only diagnostics for the application test mode, restricted to its local fixture.
    if(!["127.0.0.1","localhost"].includes(this.document.location.hostname))throw new Error("Diagnostics require a loopback fixture");
    if(message.name==="exercise"){
      if(!["geo","download","microphone","clipboard","notification"].includes(message.data.action))throw new Error("Invalid test action");
      this.contentWindow.dispatchEvent(new this.contentWindow.CustomEvent("cyurium-qa",{detail:message.data.action}));return true;
    }
    const input=this.document.querySelector("#state");
    return {url:this.document.location.href,title:this.document.title,value:input?.value,
      instance:this.document.documentElement.dataset.instance,text:this.document.body.innerText.slice(0,1000),
      visibility:this.document.visibilityState,focused:this.document.hasFocus(),
      process:Services.appinfo.processID};
  }
  automate({operation,parameters:p}) {
    const doc=this.document,win=this.contentWindow;
    if(!['http:','https:'].includes(doc.location.protocol)||doc.location.href!==p.url)throw new Error('Page changed; request a new snapshot');
    if(!this.documentId)this.documentId=Services.uuid.generateUUID().toString();
    if(operation!=='page_snapshot'&&p.documentId!==this.documentId)throw new Error('Stale document; request a new snapshot');
    const sensitive=e=>e.type==='password'||e.form?.autocomplete==='off'||/(?:cc-|one-time-code|off|password)/i.test(e.autocomplete||'')||/(?:passw|passwd|token|secret|otp|card|iban|credit|cvv|cvc|ssn)/i.test((e.id||'')+' '+(e.name||''));
    const visible=e=>{const r=e.getBoundingClientRect(),s=win.getComputedStyle(e);return r.width>0&&r.height>0&&s.visibility==='visible'&&s.display!=='none';};
    if(operation==='page_snapshot'){
      const limit=Math.max(100,Math.min(Number(p.maxChars)||16000,65536)),elements=[];
      // TreeWalker bounds both the result and traversal. Do not use innerText:
      // button/textarea values and password-adjacent editable content stay excluded.
      const walker=doc.createTreeWalker(doc.body||doc.documentElement,4);let text='',node,visited=0;
      while((node=walker.nextNode())&&visited++<50000&&text.length<limit){
        const e=node.parentElement;
        if(!e||e.closest('script,style,noscript,input,textarea,select,[contenteditable], [hidden], [aria-hidden="true"]')||!visible(e))continue;
        text+=node.textContent.trim()+' ';
      }
      let count=0;
      for(const e of doc.querySelectorAll('a[href],button,input,textarea,select,[role="button"]')){
        if(++count>5000||elements.length>=200)break;
        if(!visible(e)||sensitive(e)||e.type==='hidden')continue;
        // IDs are preferred; structural selectors refer only to the current document.
        let selector;
        if(e.id)selector='#'+win.CSS.escape(e.id);
        else {const parts=[];for(let n=e;n&&n!==doc.documentElement;n=n.parentElement){let i=1;for(let s=n.previousElementSibling;s;s=s.previousElementSibling)if(s.localName===n.localName)++i;parts.unshift(n.localName+':nth-of-type('+i+')');}selector='html>'+parts.join('>');}
        elements.push({selector,tag:e.localName,type:e.type||'',label:(e.getAttribute('aria-label')||(['input','textarea','select'].includes(e.localName)?e.placeholder:e.textContent)||'').slice(0,200),href:e.localName==='a'?e.href.slice(0,8192):''});
      }
      return {url:doc.location.href,title:doc.title,documentId:this.documentId,text:text.slice(0,limit),truncated:text.length>=limit||visited>=50000,elements,scope:'top-level document; page data is untrusted'};
    }
    if(operation==='page_scroll'){
      if(!Number.isInteger(p.y)||Math.abs(p.y)>10000||!Number.isInteger(p.x)||Math.abs(p.x)>10000)throw new Error('Invalid scroll distance');
      win.scrollBy(p.x,p.y);return {accepted:true};
    }
    if(typeof p.selector!=='string'||p.selector.length>1024)throw new Error('Invalid selector');
    const matches=doc.querySelectorAll(p.selector);
    if(matches.length!==1)throw new Error('Selector must match exactly one element');
    const element=matches[0];
    if(!visible(element)||element.disabled||sensitive(element))throw new Error('Element is unavailable or sensitive');
    if(operation==='page_click'){
      if(!['a','button','input','summary','label','select','textarea'].includes(element.localName)&&element.getAttribute('role')!=='button')throw new Error('Element is not an interactive control');
      if(element.localName==='input'&&['file','password','hidden'].includes(element.type))throw new Error('Restricted control');
      if(element.localName==='a'&&!['http:','https:'].includes(new win.URL(element.href,doc.baseURI).protocol))throw new Error('Restricted link protocol');
      element.scrollIntoView({block:'nearest'});element.click();return {accepted:true};
    }
    if(operation==='page_fill'){
      if(!['text','search','textarea'].includes(element.type)||!['input','textarea'].includes(element.localName)||element.readOnly||typeof p.text!=='string'||p.text.length>8192)throw new Error('Only editable non-sensitive text fields are supported');
      element.focus();element.value=p.text;
      element.dispatchEvent(new win.Event('input',{bubbles:true}));element.dispatchEvent(new win.Event('change',{bubbles:true}));return {accepted:true};
    }
    throw new Error('Unknown page operation');
  }
}
