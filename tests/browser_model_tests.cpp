// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_model.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace cyurium;
void check(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
int main(){try{
    BrowserModel browser;
    auto first=browser.selected;
    browser.navigate("example.org");browser.navigate("example.net/path");browser.back();
    check(browser.active().current().url=="https://example.org","Back history is wrong");
    check(browser.active().can_forward(),"Forward history was lost");
    browser.navigate("nouvelle recherche été");
    check(!browser.active().can_forward(),"Branching navigation kept stale forward history");
    check(browser.active().current().url.find("%C3%A9t%C3%A9")!=std::string::npos,"UTF-8 search must be encoded");
    browser.new_tab();auto second=browser.selected;
    browser.navigate("mozilla.org");browser.select(first);browser.close(second);
    check(browser.selected==first,"Closing a background tab changed selection");
    browser.new_tab();browser.close(browser.selected);check(browser.selected==first,"Adjacent tab was not selected");
    browser.close(first);check(browser.tabs.empty()&&browser.selected==0,"Closing the last tab must empty the window");browser.new_tab();
    browser.navigate("example.org");auto size=browser.profile->bookmarks.size();browser.toggle_bookmark();
    check(browser.bookmarked()&&browser.profile->bookmarks.size()==size+1,"Bookmark was not added");
    browser.toggle_bookmark();check(!browser.bookmarked(),"Bookmark was not removed");
    const auto content_tab=browser.selected;
    browser.open(Page::Settings);browser.toggle_bookmark();check(browser.profile->bookmarks.size()==size,"Internal page was bookmarked");
    check(browser.selected!=content_tab&&browser.active().history.size()==1,"Settings must have a new tab and independent history");
    browser.back();check(browser.active().current().page==Page::Settings,"Settings back navigation revealed another tab's page");
    browser.close(browser.selected);check(browser.selected==content_tab&&browser.active().current().page==Page::Pending,"Closing settings did not return to the original tab");
    auto path=std::filesystem::current_path()/"test-profile"/u8"profil-éà"/"settings.txt";
    browser.profile->theme=Theme::Dark;browser.profile->ui_scale=1.2f;browser.profile->bookmarks_visible=false;
    check(browser.save(path),"Settings save failed");BrowserModel restored;
    check(restored.load(path),"Settings load failed");
    check(restored.profile->theme==Theme::Dark&&!restored.profile->bookmarks_visible&&restored.profile->ui_scale==1.2f,"Settings did not round-trip");
    check(restored.profile->entered_addresses==browser.profile->entered_addresses,"Entered addresses did not round-trip");
    check(browser.save(path),"Replacing existing settings failed");
    {std::ofstream corrupt(path);corrupt<<"CYURIUM 1\n9 1 1.0\n";}
    check(!restored.load(path)&&restored.profile->theme==Theme::Dark,"Invalid settings must not corrupt live state");
    BrowserModel live;live.navigate("first.example");auto live_id=live.selected;
    check(live.active().web_commands.back().action==WebAction::Load,"Web navigation did not enqueue a Gecko load");
    live.web_state(live_id,{true,false,false,false,"https://first.example/","First page",""},WebAction::Load,true);
    check(live.active().current().title=="First page","The engine title was not reflected in the tab");
    live.navigate("second.example");live.web_state(live_id,{true,false,true,false,"https://second.example/","Second page",""},WebAction::Load,true);
    auto cursor=live.active().cursor;live.back();
    check(live.active().cursor==cursor&&live.active().web_commands.back().action==WebAction::Back,"Back must use Gecko history while a live page is open");
    live.web_state(live_id,{true,false,false,true,"https://first.example/","First page",""},WebAction::Back,true);
    check(live.active().cursor<cursor&&live.active().can_forward(),"The engine back result did not reconcile tab history");
    live.forward();check(live.active().web_commands.back().action==WebAction::Forward,"Forward did not use Gecko history");
    live.reload();check(live.active().web_commands.back().action==WebAction::Reload,"Reload was not sent to Gecko");
    live.stop();check(live.active().web_commands.back().action==WebAction::Stop,"Stop was not sent to Gecko");
    live.open(Page::Settings);live.web_state(live_id,{true,false,false,false,"https://first.example/","Late title",""});
    check(live.active().current().page==Page::Settings,"A background web event replaced an internal page");
    check(live.selected!=live_id&&!live.active().web.connected&&live.active().web_commands.empty(),"Settings inherited the previous web document");
    BrowserModel management;management.navigate("https://video.example/watch");
    const auto video=management.selected;
    management.web_state(video,{true,false,true,false,"https://video.example/watch","Playing video",""});
    management.active().session_state={{"document","original-video-document"}};
    const auto original_cursor=management.active().cursor,original_history=management.active().history.size(),original_commands=management.active().web_commands.size();
    for(auto page:{Page::Settings,Page::Bookmarks,Page::History,Page::Downloads,Page::Extensions}){
        management.open(page);
        check(management.tabs.size()==2&&management.selected!=video&&management.active().current().page==page,"Management page did not get a distinct selected tab");
        check(management.active().history.size()==1&&!management.active().can_back()&&management.active().session_state.is_null(),"Management tab inherited web history or document state");
        const auto internal=management.selected;management.select(video);
        check(management.active().current().url=="https://video.example/watch"&&management.active().current().title=="Playing video"&&management.active().web.connected,"Management page replaced the video tab");
        check(management.active().cursor==original_cursor&&management.active().history.size()==original_history&&management.active().web_commands.size()==original_commands&&management.active().session_state["document"]=="original-video-document","Management page changed the original document, history or pending web commands");
        management.select(internal);management.close(internal);check(management.selected==video,"Closing management page did not select the video tab");
    }
    management.navigate("cyurium://settings");
    check(management.tabs.size()==2&&management.selected!=video&&management.active().current().page==Page::Settings,"Typed management URL replaced the original tab");
    std::cout<<"PASS: independent tabs, close lifetime, history branching, UTF-8 search, bookmarks, settings and corrupt-profile recovery\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
