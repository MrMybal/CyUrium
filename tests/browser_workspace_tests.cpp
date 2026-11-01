// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_workspace.hpp"
#include "tab_drag.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace cyurium;
void check(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
int main(){try {
    BrowserWorkspace app;auto a=app.create_window(),b=app.create_window();
    auto* first=app.find(a);first->navigate("first.example");first->navigate("second.example");first->back();
    const auto moving=first->selected;first->new_tab();const auto remaining=first->selected;
    check(first->reorder(moving,2)&&first->tabs.back().id==moving,"Reorder did not move the same tab");
    check(first->selected==remaining,"Reorder changed selection");
    check(app.move_tab(a,b,moving,0),"Transfer failed");
    check(app.find(b)->selected==moving,"The transferred tab was not selected");
    check(app.find(b)->active().history.size()==3&&app.find(b)->active().can_forward(),"Transfer lost navigation history");
    check(app.find(b)->active().current().url=="https://first.example","Transfer recreated the destination");
    app.find(b)->forward();check(app.find(b)->active().current().url=="https://second.example","Transferred history is unusable");
    check(app.find(a)->selected==remaining,"Transfer changed an unrelated selection");
    app.find(a)->profile->theme=Theme::Dark;
    check(app.find(b)->profile->theme==Theme::Dark,"Profile settings must be shared");
    auto c=app.detach_tab(a,remaining);check(c&&app.size()==2&&!app.find(a),"Detaching the last tab must close its source window");
    check(app.owner(remaining)==c,"Detached tab lost ownership");
    check(!app.move_tab(999,b,moving,0)&&!app.move_tab(b,c,999,0),"Stale transfers must be rejected");
    std::set<TabId> ids;for(auto w:app.window_ids())for(const auto& t:app.find(w)->tabs)check(ids.insert(t.id).second,"Tab IDs collided across windows");
    check(app.close_window(c)&&app.find(b),"Closing one window affected another");
    BrowserModel different;check(!app.find(b)->transfer_to(different,moving,0),"Tabs must not cross isolated profiles");

    WindowDropZone left{1,{{0,0,600,52},{{10,{40,7,200,37},{213,15,22,22},0},{11,{240,7,200,37},{413,15,22,22},1}},2}};
    WindowDropZone right{2,{{700,100,600,52},{{20,{740,107,200,37},{913,115,22,22},0}},1}};
    std::vector<WindowDropZone> zones{left,right};TabDragController drag;
    drag.update({{100,20},true,true,false,false},zones);
    drag.update({{103,21},false,true,false,false},zones);check(!drag.visual().active,"A click must not start a drag");
    drag.update({{450,20},false,true,false,false},zones);check(drag.visual().active&&drag.visual().before==2,"Reorder preview is wrong");
    auto drop=drag.update({{450,20},false,false,true,false},zones);
    check(drop&&drop->source==1&&drop->target==1&&drop->tab==10&&drop->before==2,"In-strip drop is wrong");
    drag.update({{100,20},true,true,false,false},zones);drag.update({{760,120},false,true,false,false},zones);
    drop=drag.update({{760,120},false,false,true,false},zones);check(drop&&drop->target==2&&drop->before==0,"Cross-window target is wrong");
    drag.update({{100,20},true,true,false,false},zones);drag.update({{500,300},false,true,false,false},zones);
    drop=drag.update({{500,300},false,false,true,false},zones);check(drop&&drop->target==0,"Dropping outside must detach");
    drag.update({{100,20},true,true,false,false},zones);drag.update({{500,300},false,true,false,true},zones);
    check(!drag.tracking(),"Escape must cancel a drag");
    drag.update({{220,20},true,true,false,false},zones);check(!drag.tracking(),"A close button must not grab the tab");
    drag.update({{100,20},true,true,false,false},zones);zones.erase(zones.begin());
    check(!drag.update({{760,120},false,false,true,false},zones)&&!drag.tracking(),"Closing a drag source must cancel safely");
    // A foreground page must shield the strip of a window underneath it.
    left.bounds={0,0,600,400};right.bounds={60,0,600,400};right.strip.drop_area.y=-100;
    zones={left,right};drag.update({{100,20},true,true,false,false},zones);
    zones={right,left};drag.update({{120,20},false,true,false,false},zones);
    drop=drag.update({{120,20},false,false,true,false},zones);
    check(drop&&drop->target==0,"A hidden tab strip accepted the drop through another window");
    std::cout<<"PASS: stable tab IDs, reorder, move/detach with full history, shared profile, independent closure, drag thresholds, target slots and cancellation\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
