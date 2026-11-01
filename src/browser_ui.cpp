// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_ui.hpp"
#include "browser_theme.hpp"
#include <cyui/Widgets.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <stdexcept>

namespace cyurium {
namespace {
struct Palette {
    ImU32 strip, surface, field, text, muted, line, hover, accent, tint, page, soft;
};
Palette from_roles(const cyui::Palette& p) {
    return {p.bg0,p.bg2,p.bg3,p.text,p.textDim,p.line,p.bg4,p.accent,p.accentDim,p.bg1,p.bg5};
}
Palette palette(bool) { return from_roles(cyui::palette()); }
void native_popup(const char* id) {
    if(!(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_ViewportsEnable)||!ImGui::IsPopupOpen(id))return;
    ImGuiWindowClass wc;wc.ParentViewportId=ImGui::GetWindowViewport()->ID;
    wc.ViewportFlagsOverrideSet=ImGuiViewportFlags_NoAutoMerge|ImGuiViewportFlags_NoDecoration|ImGuiViewportFlags_NoTaskBarIcon;
    ImGui::SetNextWindowClass(&wc);
}
bool browser_popup(const char* id) {
    const bool native=(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_ViewportsEnable)!=0;
    native_popup(id);
    // ImGui normally keeps ordinary popups inside the parent viewport even
    // with NoAutoMerge. A modal popup owns its viewport and stays above the
    // native Gecko child surface. Restore normal menu dismissal explicitly.
    const bool open=ImGui::BeginPopup(id,native?ImGuiWindowFlags_Modal:ImGuiWindowFlags_None);
    if(open&&native&&!ImGui::IsWindowAppearing()){
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize(),mouse=ImGui::GetIO().MousePos;
        const bool outside=mouse.x<pos.x||mouse.y<pos.y||mouse.x>=pos.x+size.x||mouse.y>=pos.y+size.y;
        if((outside&&(ImGui::IsMouseClicked(0)||ImGui::IsMouseClicked(1)))||ImGui::IsKeyPressed(ImGuiKey_Escape)||ImGui::GetIO().AppFocusLost)ImGui::CloseCurrentPopup();
    }
    return open;
}

enum class Icon { Back, Forward, Reload, Home, Plus, Close, Minus, Maximize, Restore, Search,
    Globe, Star, Menu, Chevron, Download, Puzzle, Settings, Clock, Folder, Sun, Moon, Arrow, Check, Monitor, Lock, Warning, Chat };
struct Paint {
    float s;
    Palette p;
    ImVec2 origin{};
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImVec2 pt(float x, float y) const { auto q=cyui::dp(x,y);return {q.x+origin.x,q.y+origin.y}; }
    ImVec2 size(float x,float y) const { return {x*s,y*s}; }
    void box(float x,float y,float w,float h,ImU32 color,float radius=0) {
        d->AddRectFilled(pt(x,y),pt(x+w,y+h),color,radius*s);
    }
    void border(float x,float y,float w,float h,ImU32 color,float radius=0) {
        d->AddRect(pt(x,y),pt(x+w,y+h),color,radius*s,0,s);
    }
    void line(float x,float y,float xx,float yy,ImU32 color,float weight=1.5f) {
        d->AddLine(pt(x,y),pt(xx,yy),color,weight*s);
    }
    void label(float x,float y,const char* value,float size,ImU32 color) {
        d->AddText(ImGui::GetFont(),size*s,pt(x,y),color,value);
    }
    void center(float x,float y,float w,const char* value,float size,ImU32 color) {
        const float measured=ImGui::GetFont()->CalcTextSizeA(size*s,10000,0,value).x/s;
        label(x+(w-measured)*.5f,y,value,size,color);
    }
    void clipped(float x,float y,float w,const char* value,float size,ImU32 color) {
        d->PushClipRect(pt(x,y),pt(x+w,y+size+5),true);label(x,y,value,size,color);d->PopClipRect();
    }
    void icon(Icon icon,float cx,float cy,float size,ImU32 color) {
        const float k=size/20.f;
        auto point=[&](float x,float y){return pt(cx+(x-10)*k,cy+(y-10)*k);};
        auto ln=[&](float x,float y,float xx,float yy){d->AddLine(point(x,y),point(xx,yy),color,1.55f*s);};
        auto circle=[&](float x,float y,float r){d->AddCircle(point(x,y),r*k*s,color,24,1.5f*s);};
        auto rect=[&](float x,float y,float xx,float yy,float r=1){d->AddRect(point(x,y),point(xx,yy),color,r*k*s,0,1.5f*s);};
        switch(icon) {
        case Icon::Back: ln(15,10,5,10);ln(5,10,10,5);ln(5,10,10,15);break;
        case Icon::Forward: ln(5,10,15,10);ln(15,10,10,5);ln(15,10,10,15);break;
        case Icon::Reload:
            d->PathArcTo(point(10,10),6*k*s,.3f,5.35f,24);d->PathStroke(color,0,1.5f*s);
            ln(14,3,15,8);ln(15,8,10,7);break;
        case Icon::Home: ln(3,9,10,3);ln(10,3,17,9);ln(5,8,5,17);ln(5,17,15,17);ln(15,17,15,8);ln(8,17,8,12);ln(8,12,12,12);ln(12,12,12,17);break;
        case Icon::Chat: rect(3,3,17,14,3);ln(6,14,6,18);ln(6,18,10,14);ln(6,7,14,7);ln(6,10,11,10);break;
        case Icon::Plus: ln(4,10,16,10);ln(10,4,10,16);break;
        case Icon::Close: ln(5,5,15,15);ln(15,5,5,15);break;
        case Icon::Minus: ln(4,10,16,10);break;
        case Icon::Maximize: rect(5,5,15,15,0);break;
        case Icon::Restore: rect(4,7,13,16,0);ln(7,7,7,4);ln(7,4,16,4);ln(16,4,16,13);ln(16,13,13,13);break;
        case Icon::Search: circle(8.5f,8.5f,5.5f);ln(13,13,17,17);break;
        case Icon::Globe: circle(10,10,7);ln(3,10,17,10);d->AddEllipse(point(10,10),{3*k*s,7*k*s},color,0,24,1.4f*s);break;
        case Icon::Star: {
            ImVec2 points[10];for(int i=0;i<10;++i){float a=-1.5707963f+i*.62831853f,r=i%2?3.6f:7.5f;points[i]=point(10+std::cos(a)*r,10+std::sin(a)*r);}
            d->AddPolyline(points,10,color,ImDrawFlags_Closed,1.5f*s);break;}
        case Icon::Menu: for(int i=0;i<3;++i)d->AddCircleFilled(point(10,float(4+i*6)),1.25f*k*s,color,8);break;
        case Icon::Chevron: ln(5,7,10,12);ln(10,12,15,7);break;
        case Icon::Download: ln(10,3,10,13);ln(6,9,10,13);ln(10,13,14,9);ln(4,14,4,17);ln(4,17,16,17);ln(16,17,16,14);break;
        case Icon::Puzzle: rect(4,4,16,16,2);circle(10,3,2.3f);circle(17,10,2.3f);break;
        case Icon::Settings:
            circle(10,10,5.7f);circle(10,10,2);for(int i=0;i<8;++i){float a=i*.785398f;ln(10+std::cos(a)*6,10+std::sin(a)*6,10+std::cos(a)*8,10+std::sin(a)*8);}break;
        case Icon::Clock: circle(10,10,7);ln(10,5,10,10);ln(10,10,14,12);break;
        case Icon::Folder: ln(3,6,3,16);ln(3,16,17,16);ln(17,16,17,6);ln(17,6,10,6);ln(10,6,8,3);ln(8,3,3,3);ln(3,3,3,6);break;
        case Icon::Sun:
            circle(10,10,3.5f);for(int i=0;i<8;++i){float a=i*.785398f;ln(10+std::cos(a)*6,10+std::sin(a)*6,10+std::cos(a)*8,10+std::sin(a)*8);}break;
        case Icon::Moon: d->PathArcTo(point(10,10),7*k*s,.2f,4.6f,24);d->PathStroke(color,0,1.5f*s);ln(9,3,8,8);ln(8,8,12,12);ln(12,12,17,11);break;
        case Icon::Arrow: ln(4,10,16,10);ln(16,10,11,5);ln(16,10,11,15);break;
        case Icon::Check: ln(4,10,8,14);ln(8,14,16,5);break;
        case Icon::Monitor: rect(3,3,17,13);ln(10,13,10,17);ln(6,17,14,17);break;
        case Icon::Lock: rect(4,9,16,18,2);d->PathArcTo(point(10,8),4*k*s,3.14159f,6.28318f,16);d->PathStroke(color,0,1.4f*s);ln(10,12,10,15);break;
        case Icon::Warning: ln(10,3,18,17);ln(18,17,2,17);ln(2,17,10,3);ln(10,7,10,11);circle(10,14,.6f);break;
        }
    }
    bool hit(const char* id,float x,float y,float w,float h,bool enabled=true) {
        ImGui::SetCursorScreenPos(pt(x,y));
        ImGui::PushStyleColor(ImGuiCol_Button,0);ImGui::PushStyleColor(ImGuiCol_ButtonHovered,0);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,0);
        ImGui::BeginDisabled(!enabled);
        bool result=ImGui::Button(id,size(w,h));
        ImGui::EndDisabled();ImGui::PopStyleColor(3);return result;
    }
    bool button(const char* id,Icon glyph,float x,float y,const char* tip,bool enabled=true) {
        bool pressed=hit(id,x,y,32,32,enabled);
        if(ImGui::IsItemHovered()) {
            box(x,y,32,32,p.hover,16);
            if(tip && ImGui::BeginItemTooltip()){
#ifdef __APPLE__
                std::string hint=tip;auto at=hint.find("Ctrl+");if(at!=std::string::npos)hint.replace(at,5,"Cmd+");
                ImGui::TextUnformatted(hint.c_str());
#else
                ImGui::TextUnformatted(tip);
#endif
                ImGui::EndTooltip();
            }
        }
        icon(glyph,x+16,y+16,18,enabled?p.text:p.muted);return pressed;
    }
    void logo(float x,float y,float radius) {
        const auto texture=browser_logo();
        if(texture.GetTexID())d->AddImage(texture,pt(x-radius,y-radius),pt(x+radius,y+radius));
    }
};
}

UiActions BrowserUi::draw(BrowserModel& model,float dpi,bool maximized) {
    ViewFrame frame;frame.dpi=dpi;frame.maximized=maximized;
    frame.bounds={0,0,ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y};
    return draw(model,frame);
}
UiActions BrowserUi::draw(BrowserModel& model,const ViewFrame& frame) {
    frame_=frame;model_=&model;scale_=frame.dpi*model.profile->ui_scale;actions_={};editing_=false;
    const auto p=palette(model.profile->theme==Theme::Dark);
    ImGui::PushFont(nullptr,14);
    if(frame.primary) {
        auto* main=ImGui::GetMainViewport();ImGui::SetNextWindowViewport(main->ID);
        ImGui::SetNextWindowPos(main->Pos);ImGui::SetNextWindowSize(main->Size);
    } else {
        ImGuiWindowClass wc;wc.ParentViewportId=0;
        wc.ViewportFlagsOverrideSet=ImGuiViewportFlags_NoAutoMerge|ImGuiViewportFlags_NoDecoration;
        wc.ViewportFlagsOverrideClear=ImGuiViewportFlags_NoTaskBarIcon;
        ImGui::SetNextWindowClass(&wc);
        ImGui::SetNextWindowPos({frame.bounds.x,frame.bounds.y},frame.place?ImGuiCond_Always:ImGuiCond_Once);
        ImGui::SetNextWindowSize({frame.bounds.w,frame.bounds.h},frame.place?ImGuiCond_Always:ImGuiCond_Once);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    bool open=true;
    std::string name=model.active().current().title+" - CyUrium###browser-"+std::to_string(frame.window);
    ImGui::Begin(name.c_str(),&open,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar);
    actions_.close=!open;
    origin_=ImGui::GetWindowPos();const auto size=ImGui::GetWindowSize();
    actions_.viewport=ImGui::GetWindowViewport()->ID;
    actions_.bounds={origin_.x,origin_.y,size.x,size.y};
    const float width=size.x/scale_,height=size.y/scale_;
    Paint paint{scale_,p,origin_};paint.box(0,0,width,height,p.page);
    tabs(width,frame.maximized);toolbar(width);
    float top=96;
    if(model.profile->bookmarks_visible){bookmarks(width);top+=34;}
    paint.line(0,top,width,top,p.line,1);
    const Page page=model.active().current().page;
    const float panel=model.profile->services.assistant_visible?std::clamp(assistant_width_,300.f,std::max(300.f,width-360)):0;
    const float page_width=width-panel;
    actions_.content={origin_.x,origin_.y+top*scale_,page_width*scale_,std::max(1.f,size.y-top*scale_)};
    ImGui::SetCursorScreenPos({origin_.x,origin_.y+top*scale_});
    ImGui::BeginChild("page-content",{page_width*scale_,std::max(1.f,(height-top)*scale_)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    const float minimum=page==Page::Settings?540.f:page==Page::Home?520.f:page==Page::Pending?430.f:0.f;
    const float content_height=std::max(height-top,minimum);
    const float page_top=top-ImGui::GetScrollY()/scale_;
    switch(page) {
    case Page::Home: home(page_width,page_top,content_height);break;
    case Page::Settings: settings(page_width,page_top,content_height);break;
    case Page::Pending: pending(page_width,page_top,content_height);break;
    default: library(page,page_width,page_top,content_height);break;
    }
    ImGui::SetCursorScreenPos({origin_.x,origin_.y+(page_top+content_height)*scale_-1});ImGui::Dummy({1,1});
    ImGui::EndChild();
    if(panel){
        ImGui::SetCursorScreenPos({origin_.x+page_width*scale_,origin_.y+top*scale_});
        ImGui::InvisibleButton("##assistant-splitter",{5*scale_,(height-top)*scale_});
        if(ImGui::IsItemActive())assistant_width_=std::clamp(assistant_width_-ImGui::GetIO().MouseDelta.x/scale_,300.f,std::max(300.f,width-360));
        if(ImGui::IsItemHovered()||ImGui::IsItemActive())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        assistant_panel(page_width+6,top+8,panel-14,height-top-16);
    }
    menu(width);
    permission_prompt();
    ImGui::End();ImGui::PopStyleVar(2);ImGui::PopFont();
    return actions_;
}

void BrowserUi::tabs(float width,bool maximized) {
    auto& model=*model_;Paint p{scale_,palette(model.profile->theme==Theme::Dark),origin_};
    p.box(0,0,width,44,p.p.strip);
    float left=10;
#ifdef __APPLE__
    const ImU32 traffic[]={cyui::palette().error,cyui::palette().warning,cyui::palette().success};
    for(int i=0;i<3;++i){bool pressed=p.hit(("##traffic"+std::to_string(i)).c_str(),12+i*21,12,16,20);
        p.d->AddCircleFilled(p.pt(20+i*21,22),6*scale_,traffic[i]);
        if(pressed){if(i==0)actions_.close=true;else if(i==1)actions_.minimize=true;else actions_.maximize=true;}}
    left=90;
#endif
    if(p.button("##tablist",Icon::Chevron,left,7,"Tous les onglets"))ImGui::OpenPopup("tab-list");
    if(browser_popup("tab-list")) {
        for(const auto& tab:model.tabs){ImGui::PushID(int(tab.id));if(ImGui::MenuItem(tab.current().title.c_str(),nullptr,tab.id==model.selected))model.select(tab.id);ImGui::PopID();}
        ImGui::EndPopup();
    }
    left+=38;
#ifdef __APPLE__
    float end=width-50;
#else
    float end=width-180;
#endif
    const float available=std::max(100.f,end-left-38);
    const float tab_width=std::clamp(available/float(model.tabs.size()),90.f,238.f);
    const std::size_t count=std::max<std::size_t>(1,static_cast<std::size_t>(available/tab_width));
    auto selected=std::find_if(model.tabs.begin(),model.tabs.end(),[&](const Tab& t){return t.id==model.selected;});
    const std::size_t index=static_cast<std::size_t>(selected-model.tabs.begin());
    const std::size_t first=index>=count?index-count+1:0;
    actions_.tab_strip.drop_area={origin_.x+left*scale_,origin_.y, std::max(40.f,end-left+20)*scale_,52*scale_};
    actions_.tab_strip.total=model.tabs.size();
    float x=left;
    for(std::size_t i=first;i<std::min(model.tabs.size(),first+count);++i) {
        const auto& tab=model.tabs[i];const auto id=tab.id;bool active=id==model.selected;
        actions_.tab_strip.slots.push_back({id,
            {origin_.x+x*scale_,origin_.y+7*scale_,(tab_width-4)*scale_,37*scale_},
            {origin_.x+(x+tab_width-33)*scale_,origin_.y+15*scale_,22*scale_,22*scale_},i});
        ImGui::PushID(int(id));
        // Keep the body and its close control disjoint so neither consumes
        // the other's active/hovered ID in Dear ImGui.
        bool clicked=p.hit("##tab",x,7,tab_width-33,37);
        bool hovered=ImGui::IsMouseHoveringRect(p.pt(x,7),p.pt(x+tab_width-4,44));
        if(active)p.box(x,7,tab_width-4,40,p.p.surface,11);
        else if(hovered)p.box(x,8,tab_width-4,33,p.p.hover,9);
        else if(i>first)p.line(x-2,15,x-2,32,p.p.line,1);
        p.logo(x+19,25,7);
        p.clipped(x+35,17,tab_width-72,tab.current().title.c_str(),12.5f,
            frame_.drag.active&&frame_.drag.tab==id?p.p.muted:p.p.text);
        if(clicked)model.select(id);
        if(ImGui::IsItemClicked(ImGuiMouseButton_Middle)){actions_.close_tab=id;ImGui::PopID();break;}
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right))ImGui::OpenPopup("tab-context");
        if(browser_popup("tab-context")){
            if(ImGui::MenuItem("Déplacer vers une nouvelle fenêtre"))actions_.detach_tab=id;
            if(ImGui::MenuItem("Fermer l'onglet","Ctrl+W")){actions_.close_tab=id;ImGui::EndPopup();ImGui::PopID();break;}
            ImGui::EndPopup();
        }
        if(p.hit("##close-tab",x+tab_width-33,15,22,22)) {actions_.close_tab=id;ImGui::PopID();break;}
        if(ImGui::IsItemClicked(ImGuiMouseButton_Middle)){actions_.close_tab=id;ImGui::PopID();break;}
        if(ImGui::IsItemHovered())p.box(x+tab_width-33,15,22,22,p.p.hover,11);
        p.icon(Icon::Close,x+tab_width-22,26,12,p.p.muted);
        ImGui::PopID();x+=tab_width;
    }
    if(p.button("##newtab",Icon::Plus,x+3,9,"Nouvel onglet (Ctrl+T)")){model.new_tab();focus_address_=true;}
    if(frame_.drag.active&&frame_.drag.target==frame_.window) {
        const float marker=actions_.tab_strip.marker(frame_.drag.before);
        p.d->AddLine({marker,origin_.y+10*scale_},{marker,origin_.y+40*scale_},p.p.accent,3*scale_);
    }
    actions_.drag_start=(x+42)*scale_;actions_.drag_end=(width-146)*scale_;
#ifndef __APPLE__
    const Icon icons[]={Icon::Minus,maximized?Icon::Restore:Icon::Maximize,Icon::Close};
    for(int i=0;i<3;++i) {
        const float bx=width-138+i*46;
        bool clicked=p.hit(("##window"+std::to_string(i)).c_str(),bx,0,46,43);
        if(ImGui::IsItemHovered())p.box(bx,0,46,43,i==2?cyui::palette().error:p.p.hover);
        p.icon(icons[i],bx+23,21,16,(i==2&&ImGui::IsItemHovered())?cyui::palette().accentText:p.p.muted);
        if(clicked){if(i==0)actions_.minimize=true;else if(i==1)actions_.maximize=true;else actions_.close=true;}
    }
#endif
}

void BrowserUi::toolbar(float width) {
    auto& model=*model_;Paint p{scale_,palette(model.profile->theme==Theme::Dark),origin_};
    p.box(0,44,width,52,p.p.surface);
    if(p.button("##back",Icon::Back,9,54,"Retour (Alt+Gauche)",model.active().can_back()))model.back();
    if(p.button("##forward",Icon::Forward,45,54,"Avancer (Alt+Droite)",model.active().can_forward()))model.forward();
    if(p.button("##reload",model.active().web.loading?Icon::Close:Icon::Reload,81,54,model.active().web.loading?"Arrêter le chargement":"Recharger (Ctrl+R)")){
        if(model.active().web.loading)model.stop();else model.reload();
    }
    const float x=126,w=width-x-196;
    p.box(x,52,w,36,p.p.field,19);
    const auto& url=model.active().current().url;
    if(last_tab_!=model.selected || last_url_!=url) {
        std::snprintf(address_.data(),address_.size(),"%s",model.active().current().page==Page::Home?"":url.c_str());
        last_tab_=model.selected;last_url_=url;
    }
    const auto captures=model.profile->services.state.value("media",nlohmann::json::array());
    if(p.hit("##site-permissions",x+4,54,29,32))ImGui::OpenPopup("site-permissions");
    const auto& web=model.active().web;const bool settled=model.active().current().page==Page::Pending&&web.url==url&&!web.loading;
    const auto security=settled?web.security:std::string("unknown");const bool encrypted=security=="encrypted",warning=security=="broken"||security=="insecure";
    p.icon(!captures.empty()?Icon::Monitor:encrypted?Icon::Lock:warning?Icon::Warning:model.active().current().page==Page::Pending?Icon::Globe:Icon::Search,x+19,70,16,!captures.empty()||warning?p.p.accent:p.p.muted);
    if(ImGui::IsItemHovered())ImGui::SetTooltip(!captures.empty()?"Caméra, microphone ou partage actif":encrypted?"Connexion chiffrée · certificat accepté par Gecko":warning?"Connexion non chiffrée ou certificat non validé":"Informations et autorisations du site");
    ImGui::SetNextWindowPos(p.pt(x,94));ImGui::SetNextWindowSize(p.size(380,0));
    if(browser_popup("site-permissions")){
        ImGui::TextUnformatted("Autorisations des sites");
        if(settled&&!web.security_origin.empty())ImGui::TextWrapped("Origine : %s",web.security_origin.c_str());
        ImGui::TextWrapped("%s",encrypted?"Connexion chiffrée. Cela ne garantit pas la fiabilité du site.":security=="insecure"?"Connexion HTTP non chiffrée.":security=="broken"?"La sécurité de la connexion n’a pas été validée.":security=="local"?"Fichier local.":"Connexion en attente de validation.");
        if(settled&&!web.security_issuer.empty())ImGui::TextWrapped("Certificat émis par : %s",web.security_issuer.c_str());
        ImGui::Separator();
        for(const auto& capture:captures){ImGui::PushID(capture.at("windowID").dump().c_str());ImGui::TextWrapped("%s",capture.value("origin","").c_str());
            if(ImGui::Button("Arrêter la capture / le partage"))model.profile->services.request("media-stop",{{"tab",capture["tab"]}});ImGui::PopID();}
        if(captures.empty())ImGui::TextUnformatted("Aucune capture active.");
        if(ImGui::Button("Gérer les permissions")){settings_category_=2;model.open(Page::Settings);ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    ImGui::SetCursorScreenPos(p.pt(x+34,53));ImGui::SetNextItemWidth((w-73)*scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,p.size(3,9));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,0);ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,0);ImGui::PushStyleColor(ImGuiCol_FrameBgActive,0);
    if(focus_address_){ImGui::SetKeyboardFocusHere();focus_address_=false;}
    if(ImGui::InputTextWithHint("##address","Rechercher ou saisir une adresse",address_.data(),address_.size(),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll)) {
        model.navigate(address_.data());ImGui::SetKeyboardFocusHere(-1);
    }
    editing_=ImGui::IsItemActive();actions_.address_editing=editing_;
    if(editing_)p.border(x,52,w,36,p.p.accent,19);
    ImGui::PopStyleColor(3);ImGui::PopStyleVar();
    if(p.hit("##bookmark",x+w-35,54,30,32,model.active().current().page==Page::Pending)){model.toggle_bookmark();actions_.save=true;}
    p.icon(Icon::Star,x+w-20,70,18,model.bookmarked()?p.p.accent:p.p.muted);
    if(p.button("##assistant",Icon::Chat,width-183,54,"Conversation · Assistant"))model.profile->services.assistant_visible=!model.profile->services.assistant_visible;
    if(p.button("##extensions",Icon::Puzzle,width-145,54,"Extensions"))model.open(Page::Extensions);
    if(p.button("##downloads",Icon::Download,width-107,54,"Téléchargements (Ctrl+J)"))model.open(Page::Downloads);
    if(p.hit("##profile",width-68,56,29,29))model.open(Page::Settings);
    p.d->AddCircleFilled(p.pt(width-53.5f,70.5f),13*scale_,p.p.tint);
    p.center(width-68,62,29,"C",14,p.p.accent);
    if(p.button("##menu",Icon::Menu,width-36,54,"Personnaliser et contrôler CyUrium"))menu_requested_=true;
}

void BrowserUi::bookmarks(float width) {
    auto& model=*model_;Paint p{scale_,palette(model.profile->theme==Theme::Dark),origin_};
    p.box(0,96,width,34,p.p.surface);
    float x=14;
    for(std::size_t i=0;i<model.profile->bookmarks.size();++i) {
        const auto& b=model.profile->bookmarks[i];
        const float w=std::min(165.f,ImGui::CalcTextSize(b.title.c_str()).x/scale_+35);
        if(x+w>width-120)break;
        ImGui::PushID(int(i));bool clicked=p.hit("##favorite",x,99,w,27);
        if(ImGui::IsItemHovered())p.box(x,99,w,27,p.p.hover,6);
        p.icon(Icon::Globe,x+12,112,14,p.p.muted);p.clipped(x+25,105,w-29,b.title.c_str(),12,p.p.text);
        if(clicked)model.navigate(b.url);
        ImGui::PopID();x+=w+5;
    }
    if(p.hit("##all-bookmarks",width-110,99,100,27))model.open(Page::Bookmarks);
    p.icon(Icon::Folder,width-96,112,15,p.p.muted);p.label(width-80,105,"Tous les favoris",11.5f,p.p.muted);
}

void BrowserUi::home(float width,float top,float height) {
    auto& model=*model_;Paint p{scale_,palette(model.profile->theme==Theme::Dark),origin_};
    const float center=width*.5f,base=top+std::clamp(height*.23f,62.f,170.f);
    p.logo(center-113,base+33,29);
    p.label(center-67,base+4,"CyUrium",49,p.p.text);
    p.center(0,base+75,width,"Un nouvel onglet. Toutes les possibilités.",15,p.p.muted);
    const float w=std::min(620.f,width-64),x=center-w*.5f,y=base+131;
    p.box(x-2,y+3,w+4,54,cyui::alpha(p.p.text,.035f),28);
    p.box(x,y,w,52,p.p.surface,27);p.border(x,y,w,52,p.p.line,27);
    p.icon(Icon::Search,x+25,y+26,20,p.p.muted);
    ImGui::SetCursorScreenPos(p.pt(x+48,y+8));ImGui::SetNextItemWidth((w-101)*scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,p.size(0,10));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,0);ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,0);ImGui::PushStyleColor(ImGuiCol_FrameBgActive,0);
    if(ImGui::InputTextWithHint("##home-search","Rechercher sur le Web ou saisir une URL",search_.data(),search_.size(),ImGuiInputTextFlags_EnterReturnsTrue)) {
        if(search_[0]){model.navigate(search_.data());search_[0]=0;}
    }
    editing_=editing_||ImGui::IsItemActive();
    if(ImGui::IsItemActive())p.border(x,y,w,52,p.p.accent,27);
    ImGui::PopStyleColor(3);ImGui::PopStyleVar();
    if(p.button("##search-submit",Icon::Arrow,x+w-42,y+10,"Rechercher")&&search_[0]){model.navigate(search_.data());search_[0]=0;}
    const std::size_t count=std::min<std::size_t>(model.profile->bookmarks.size(),5);
    const float spacing=std::min(104.f,(width-40)/float(std::max<std::size_t>(count,1)));
    const float start=center-spacing*float(count)*.5f;
    for(std::size_t i=0;i<count;++i) {
        const float bx=start+i*spacing,by=y+85;
        ImGui::PushID(int(i));bool clicked=p.hit("##shortcut",bx,by,spacing-4,99);
        if(ImGui::IsItemHovered())p.box(bx,by,spacing-4,99,p.p.soft,12);
        p.d->AddCircleFilled(p.pt(bx+(spacing-4)*.5f,by+29),23*scale_,p.p.field);
        p.icon(i==0?Icon::Folder:i==1?Icon::Globe:i==2?Icon::Globe:Icon::Monitor,bx+(spacing-4)*.5f,by+29,22,i==2?p.p.accent:p.p.text);
        p.center(bx,by+65,spacing-4,model.profile->bookmarks[i].title.c_str(),12,p.p.text);
        if(clicked)model.navigate(model.profile->bookmarks[i].url);
        ImGui::PopID();
    }
    if(height>490) {
        p.box(center-120,y+225,240,28,p.p.soft,14);
        p.center(center-120,y+232,240,"Gecko · Interface native",11.5f,p.p.muted);
    }
    p.label(24,top+height-33,"CyUrium",12,p.p.muted);
    const float button_width=148;
    if(p.hit("##customize",width-button_width-22,top+height-48,button_width,32))ImGui::OpenPopup("customize");
    if(ImGui::IsItemHovered())p.box(width-button_width-22,top+height-48,button_width,32,p.p.hover,16);
    p.icon(Icon::Settings,width-button_width-4,top+height-32,16,p.p.muted);
    p.label(width-button_width+13,top+height-39,"Personnaliser",12,p.p.muted);
    ImGui::SetNextWindowPos(p.pt(width-242,top+height-180));
    if(browser_popup("customize")) {
        ImGui::TextUnformatted("Apparence");ImGui::Separator();
        if(ImGui::MenuItem("Clair",nullptr,model.profile->theme==Theme::Light)){model.profile->theme=Theme::Light;actions_.save=true;}
        if(ImGui::MenuItem("Sombre",nullptr,model.profile->theme==Theme::Dark)){model.profile->theme=Theme::Dark;actions_.save=true;}
        if(ImGui::MenuItem("Paramètres"))model.open(Page::Settings);
        ImGui::EndPopup();
    }
}

void BrowserUi::pending(float width,float top,float height) {
    Paint p{scale_,palette(model_->profile->theme==Theme::Dark),origin_};
    const float y=top+std::max(80.f,height*.24f);
    p.box(width*.5f-32,y,64,64,p.p.tint,20);p.icon(Icon::Globe,width*.5f,y+32,29,p.p.accent);
    const auto& web=model_->active().web;
    p.center(16,y+91,width-32,web.error.empty()?"Chargement de la page":"Impossible d'afficher cette page",25,p.p.text);
    p.center(16,y+135,width-32,web.error.empty()?"Démarrage du moteur Gecko…":web.error.c_str(),13,p.p.muted);
    const auto& url=model_->active().current().url;
    p.center(32,y+176,width-64,url.size()>80?(url.substr(0,77)+"...").c_str():url.c_str(),12,p.p.muted);
    if(p.hit("##return-home",width*.5f-90,y+220,180,38))model_->open(Page::Home);
    p.box(width*.5f-90,y+220,180,38,p.p.tint,19);p.center(width*.5f-90,y+231,180,"Retour à l'accueil",13,p.p.accent);
    if(!web.error.empty()){
        if(p.hit("##restart-engine",width*.5f-110,y+271,220,38))model_->profile->services.request("engine-restart");
        p.box(width*.5f-110,y+271,220,38,p.p.field,19);p.center(width*.5f-110,y+282,220,"Redémarrer le moteur",13,p.p.text);
    }
}

void BrowserUi::settings(float width,float top,float height) {
    auto& model=*model_;Paint p{scale_,palette(model.profile->theme==Theme::Dark),origin_};
    const float sidebar=width>850?222.f:178.f;
    p.box(0,top,sidebar,height,p.p.surface);p.line(sidebar,top,sidebar,top+height,p.p.line,1);
    p.label(27,top+33,"Paramètres",24,p.p.text);
    const char* categories[]={"Apparence","Navigation","Permissions","Système","À propos"};
    const Icon icons[]={Icon::Sun,Icon::Globe,Icon::Check,Icon::Settings,Icon::Monitor};
    for(int i=0;i<5;++i) {
        const float y=top+91+i*46;
        if(p.hit(("##settings-category"+std::to_string(i)).c_str(),14,y,sidebar-28,38))settings_category_=i;
        if(settings_category_==i)p.box(14,y,sidebar-28,38,p.p.tint,9);
        else if(ImGui::IsItemHovered())p.box(14,y,sidebar-28,38,p.p.hover,9);
        p.icon(icons[i],34,y+19,18,settings_category_==i?p.p.accent:p.p.muted);
        p.label(52,y+11,categories[i],13,settings_category_==i?p.p.accent:p.p.text);
    }
    const float x=sidebar+std::clamp((width-sidebar-690)*.5f,28.f,95.f),w=std::min(690.f,width-x-30),y=top+38;
    p.label(x,y,categories[settings_category_],27,p.p.text);
    if(settings_category_==0) {
        p.label(x,y+44,"Un espace qui vous ressemble.",14,p.p.muted);
        const float card=(w-18)*.5f;
        for(int i=0;i<2;++i) {
            const float xx=x+i*(card+18);const auto sample=from_roles(browser_palette(i==1));bool active=(int(model.profile->theme)==i);
            bool clicked=p.hit(("##theme-card"+std::to_string(i)).c_str(),xx,y+89,card,153);
            p.box(xx,y+89,card,153,p.p.surface,13);p.border(xx,y+89,card,153,active?p.p.accent:p.p.line,13);
            p.box(xx+11,y+100,card-22,94,sample.page,7);p.box(xx+11,y+100,card-22,17,sample.strip,6);
            p.box(xx+23,y+105,(card-60)*.65f,12,sample.surface,3);
            p.box(xx+11,y+117,card-22,19,sample.surface);p.box(xx+39,y+122,card-77,8,sample.field,4);
            p.box(xx+card*.35f,y+155,card*.3f,5,sample.line,2);
            p.label(xx+15,y+213,i==0?"Clair":"Sombre",14,p.p.text);
            if(active)p.icon(Icon::Check,xx+card-25,y+220,18,p.p.accent);
            if(clicked){model.profile->theme=static_cast<Theme>(i);actions_.save=true;}
        }
        p.box(x,y+271,w,72,p.p.surface,12);p.border(x,y+271,w,72,p.p.line,12);
        p.label(x+18,y+288,"Barre de favoris",14,p.p.text);p.label(x+18,y+312,"Garder vos sites à portée de main.",12,p.p.muted);
        ImGui::SetCursorScreenPos(p.pt(x+w-60,y+294));
        if(cyui::toggleSwitch("##toggle-bookmarks",&model.profile->bookmarks_visible))actions_.save=true;
        p.box(x,y+362,w,105,p.p.surface,12);p.border(x,y+362,w,105,p.p.line,12);
        p.label(x+18,y+378,"Taille de l'interface",14,p.p.text);
        ImGui::SetCursorScreenPos(p.pt(x+18,y+414));ImGui::SetNextItemWidth((w-36)*scale_);
        float percent=model.profile->ui_scale*100;
        if(cyui::sliderBar("##scale",&percent,85,150,"%.0f %%",(w-36)*scale_)){model.profile->ui_scale=std::round(percent)/100.f;actions_.save=true;}
    } else if(settings_category_==1) {
        service_settings(1,x,y+65,w,height-120);
    } else if(settings_category_==2||settings_category_==3) {
        service_settings(settings_category_,x,y+65,w,height-120);
    } else {
        p.logo(x+27,y+99,25);p.label(x+74,y+80,"CyUrium",30,p.p.text);
        p.label(x+75,y+120,"Version " CYURIUM_VERSION " · Desktop",13,p.p.muted);
        p.label(x,y+189,"Une interface native, commune aux trois plateformes.",15,p.p.text);
        p.label(x,y+225,"Windows, macOS et Linux",14,p.p.muted);
        p.label(x,y+257,"Interface : Dear ImGui  ·  Fenêtres et GPU : SDL3",13,p.p.muted);
        p.box(x,y+300,w,90,p.p.tint,12);
        p.label(x+18,y+318,"Moteur web",14,p.p.accent);
        p.label(x+18,y+347,"Gecko ESR 153.6 · Runtime embarqué · Rendu natif",12,p.p.text);
    }
}

void BrowserUi::library(Page page,float width,float top,float height) {
    if(page==Page::Downloads||page==Page::Extensions){service_library(page,width,top,height);return;}
    auto& model=*model_;Paint p{scale_,palette(model.profile->theme==Theme::Dark),origin_};
    const float w=std::min(830.f,width-72),x=(width-w)*.5f,y=top+44;
    const char* title=page==Page::Bookmarks?"Vos favoris":page==Page::History?"Adresses saisies":page==Page::Downloads?"Téléchargements":"Extensions";
    p.label(x,y,title,28,p.p.text);
    const char* subtitle=page==Page::Bookmarks?"Vos destinations, réunies au même endroit.":page==Page::History?"Les adresses ouvertes depuis la barre de recherche.":
        page==Page::Downloads?"Retrouvez ici vos prochains téléchargements.":"Un espace pour les extensions de votre navigateur.";
    p.label(x,y+45,subtitle,14,p.p.muted);
    ImGui::SetCursorScreenPos(p.pt(x,y+88));
    ImGui::BeginChild("library-scroll",p.size(w,std::max(80.f,height-166)),ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    p.d=ImGui::GetWindowDrawList();
    if(page==Page::Bookmarks || page==Page::History) {
        const std::size_t count=page==Page::Bookmarks?model.profile->bookmarks.size():model.profile->entered_addresses.size();
        if(count==0)ImGui::TextUnformatted("Aucune adresse pour le moment.");
        // The 58 dp card plus its 6 dp gap is the clipper's full 64 dp row.
        // Let the submitted button advance layout; a final SetCursorScreenPos
        // without an item leaves unregistered content bounds and asserts in Debug.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{ImGui::GetStyle().ItemSpacing.x,6*scale_});
        ImGuiListClipper clip;clip.Begin(static_cast<int>(count),64*scale_);
        while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
            const auto pos=ImGui::GetCursorScreenPos();const float yy=(pos.y-origin_.y)/scale_;
            const std::string url=page==Page::Bookmarks?model.profile->bookmarks[i].url:model.profile->entered_addresses[i];
            const std::string name=page==Page::Bookmarks?model.profile->bookmarks[i].title:BrowserModel::resolve(url).title;
            ImGui::PushID(i);bool clicked=p.hit("##entry",x,yy,w,58);
            if(ImGui::IsItemHovered())p.box(x,yy,w,58,p.p.hover,9);
            p.icon(Icon::Globe,x+24,yy+28,20,p.p.muted);p.clipped(x+49,yy+10,w-70,name.c_str(),14,p.p.text);
            p.clipped(x+49,yy+32,w-70,url.c_str(),12,p.p.muted);
            if(clicked)model.navigate(url);
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
}

void BrowserUi::menu(float width) {
    auto& model=*model_;Paint p{scale_,palette(model.profile->theme==Theme::Dark),origin_};
    if(menu_requested_){ImGui::OpenPopup("browser-menu");menu_requested_=false;}
    ImGui::SetNextWindowPos(p.pt(width-303,91));ImGui::SetNextWindowSize(p.size(290,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,p.size(8,10));
    if(browser_popup("browser-menu")) {
        p.d=ImGui::GetWindowDrawList();
        auto row=[&](const char* title,Icon icon,const char* shortcut,auto callback) {
            std::string hint=shortcut;
#ifdef __APPLE__
            if(hint.starts_with("Ctrl+"))hint.replace(0,5,"Cmd+");
#endif
            auto pos=ImGui::GetCursorScreenPos();
            bool hit=ImGui::Selectable((std::string("##")+title).c_str(),false,0,p.size(272,35));
            p.icon(icon,(pos.x-origin_.x)/scale_+20,(pos.y-origin_.y)/scale_+17,17,p.p.muted);
            p.label((pos.x-origin_.x)/scale_+39,(pos.y-origin_.y)/scale_+9,title,13,p.p.text);
            const float length=ImGui::CalcTextSize(hint.c_str()).x/scale_;
            p.label((pos.x-origin_.x)/scale_+257-length,(pos.y-origin_.y)/scale_+10,hint.c_str(),11,p.p.muted);
            if(hit)callback();
        };
        row("Nouvel onglet",Icon::Plus,"Ctrl+T",[&]{model.new_tab();focus_address_=true;});
        row("Nouvelle fenêtre",Icon::Monitor,"Ctrl+N",[&]{actions_.new_window=true;});
        ImGui::Separator();
        row("Favoris",Icon::Star,"",[&]{model.open(Page::Bookmarks);});
        row("Adresses saisies",Icon::Clock,"Ctrl+H",[&]{model.open(Page::History);});
        row("Téléchargements",Icon::Download,"Ctrl+J",[&]{model.open(Page::Downloads);});
        row("Extensions",Icon::Puzzle,"",[&]{model.open(Page::Extensions);});
        ImGui::Separator();
        row(model.profile->theme==Theme::Light?"Passer en mode sombre":"Passer en mode clair",model.profile->theme==Theme::Light?Icon::Moon:Icon::Sun,"",[&]{model.profile->theme=model.profile->theme==Theme::Light?Theme::Dark:Theme::Light;actions_.save=true;});
        row("Paramètres",Icon::Settings,"Ctrl+,",[&]{model.open(Page::Settings);});
        ImGui::Separator();
        row("Quitter",Icon::Close,"",[&]{actions_.quit=true;});
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}
void BrowserUi::draw_drag_preview(const TabDragVisual& drag,std::string_view title,float scale) {
    if(!drag.active)return;
    ImGuiWindowClass wc;wc.ParentViewportId=0;
    wc.ViewportFlagsOverrideSet=ImGuiViewportFlags_NoAutoMerge|ImGuiViewportFlags_NoDecoration|
        ImGuiViewportFlags_NoTaskBarIcon|ImGuiViewportFlags_TopMost|ImGuiViewportFlags_NoInputs|
        ImGuiViewportFlags_NoFocusOnAppearing|ImGuiViewportFlags_NoFocusOnClick;
    ImGui::SetNextWindowClass(&wc);
    ImGui::SetNextWindowPos({drag.pointer.x+14,drag.pointer.y+16});
    ImGui::SetNextWindowSize({238*scale,40*scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10*scale,10*scale});
    ImGui::Begin("Onglet###tab-drag-preview",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextUnformatted(title.data(),title.data()+title.size());
    ImGui::End();ImGui::PopStyleVar();
}
std::unique_ptr<BrowserView> make_browser_view(std::string_view style) {
    if(style=="classic")return std::make_unique<BrowserUi>();
    throw std::invalid_argument("Unknown browser interface: "+std::string(style));
}

}
