// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_ui.hpp"
#include "browser_theme.hpp"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
using namespace cyurium;
void check(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
int main(){try {
#ifdef _MSC_VER
    // Keep assertion failures observable to CTest instead of opening a modal
    // CRT dialog that would leave unattended UI checks blocked.
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1360,880};io.DeltaTime=1.f/60;
    io.IniFilename=nullptr;io.LogFilename=nullptr;
    apply_browser_theme(false,1);io.Fonts->AddFontDefault();
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    BrowserModel model;BrowserUi ui;
    auto frame=[&](){apply_browser_theme(model.profile->theme==Theme::Dark,model.profile->ui_scale);ImGui::NewFrame();auto result=ui.draw(model,1,false);ImGui::Render();return result;};
    auto click=[&](float x,float y){io.AddMousePosEvent(x,y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);auto result=frame();frame();return result;};
    frame();frame();
    click(302,25);check(model.tabs.size()==2,"The new-tab button did not create a tab");
    auto layout=frame().tab_strip;auto close=layout.slots.back().close;
    check(click(close.x+close.w*.5f,close.y+close.h*.5f).close_tab==model.tabs.back().id,"The tab close button did not request closure");
    ui.focus_address();frame();frame();io.AddInputCharactersUTF8("example.org");frame();
    io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();
    check(model.active().current().url=="https://example.org","The address field did not submit its text");
    click(25,70);check(model.active().current().page==Page::Home,"The back button did not navigate");
    click(61,70);check(model.active().current().page==Page::Pending,"The forward button did not navigate");
    const auto web_tab=model.selected;const auto original_tabs=model.tabs.size();
    click(1340,70);click(1150,398);check(model.active().current().page==Page::Settings,"The application menu did not open settings");
    check(model.tabs.size()==original_tabs+1&&model.selected!=web_tab,"The settings menu replaced the existing web tab");
    click(830,330);check(model.profile->theme==Theme::Dark,"The dark theme card did not change the theme");
    click(960,473);check(!model.profile->bookmarks_visible,"The CyUI bookmark switch did not change its setting");
    model.close(model.selected);check(model.selected==web_tab&&model.active().current().url=="https://example.org","Closing the settings tab lost the original page");frame();
    click(1269,70);check(model.active().current().page==Page::Downloads&&model.selected!=web_tab&&model.tabs.size()==original_tabs+1,"The downloads toolbar button replaced the web tab");
    model.close(model.selected);frame();
    click(1231,70);check(model.active().current().page==Page::Extensions&&model.selected!=web_tab&&model.tabs.size()==original_tabs+1,"The extensions toolbar button replaced the web tab");
    model.close(model.selected);frame();
    click(1193,70);check(model.profile->services.assistant_visible,"The native conversation button did not open its panel");
    check(frame().content.w<1100,"The conversation panel did not reserve space beside Gecko");
    model.profile->services.assistant={{"connectors",nlohmann::json::array()}, {"conversations",nlohmann::json::array()}, {"settings",{{"connector","codex"},{"transport","app-server"}}}};
    frame();frame();check(ImGui::GetDrawData()->TotalVtxCount>0,"The native conversation panel did not render");
    model.profile->services.assistant_visible=false;
    model.open(Page::Bookmarks);frame();frame();
    for(int i=0;i<200;++i){model.profile->bookmarks.push_back({"Bookmark "+std::to_string(i),"https://example.org/"+std::to_string(i)});model.profile->entered_addresses.push_back("https://example.org/"+std::to_string(i));}
    frame();frame();
    model.open(Page::History);frame();frame();
    // The same draw code must handle a smaller view at 150% scale without
    // breaking ImGui's clipping, layout or Begin/End stack invariants.
    io.DisplaySize={760,620};model.profile->ui_scale=1.5f;frame();frame();
    check(ImGui::GetDrawData()->TotalVtxCount>0,"The scaled narrow view produced no geometry");
    for(float scale:{.85f,1.f,1.25f,1.5f})for(auto page:{Page::Bookmarks,Page::History}){
        model.profile->ui_scale=scale;model.open(page);frame();frame();
        io.AddMousePosEvent(360,430);frame();io.AddMouseWheelEvent(0,-12);frame();frame();
        io.AddMouseWheelEvent(0,12);frame();frame();
        check(ImGui::GetDrawData()->TotalVtxCount>0,"A populated scrolling library did not render at this scale");
    }
    ImGui::DestroyContext();
    std::cout<<"PASS: tab clicks, editable address submission, back/forward, menu, independent management tabs, conversation panel, populated bookmark/history scrolling and 85-150% narrow layouts\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(ImGui::GetCurrentContext())ImGui::DestroyContext();return 1;}}
