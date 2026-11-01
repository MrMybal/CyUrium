// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "browser_workspace.hpp"
#include <memory>
#include <optional>
#include <string_view>

namespace cyurium {
struct Point { float x=0,y=0; };
struct Rect {
    float x=0,y=0,w=0,h=0;
    bool contains(Point p) const { return p.x>=x&&p.y>=y&&p.x<x+w&&p.y<y+h; }
};
struct TabSlot { TabId id=0; Rect body, close; std::size_t index=0; };
struct TabStripLayout {
    Rect drop_area;
    std::vector<TabSlot> slots;
    std::size_t total=0;
    std::size_t insertion(Point p) const;
    float marker(std::size_t before) const;
};
struct TabDragVisual {
    TabId tab=0;
    WindowId source=0,target=0;
    Point pointer,grab_offset;
    std::size_t before=0;
    bool active=false;
};
struct ViewFrame {
    WindowId window=0;
    Rect bounds;
    float dpi=1;
    bool primary=true,maximized=false,place=false;
    TabDragVisual drag;
};
struct UiActions {
    bool close=false,minimize=false,maximize=false,save=false,new_window=false,quit=false;
    TabId close_tab=0,detach_tab=0;
    float drag_start=0,drag_end=0;
    std::uint32_t viewport=0;
    Rect bounds;
    TabStripLayout tab_strip;
    Rect content;
    bool address_editing=false;
};
// A presentation adapter publishes tab hit zones. Common window/drag behavior
// is owned by the workspace/controller, not by the Chrome-style implementation.
class BrowserView {
public:
    virtual ~BrowserView()=default;
    virtual UiActions draw(BrowserModel&,const ViewFrame&)=0;
    virtual void focus_address()=0;
    virtual void show_menu()=0;
    virtual bool editing() const=0;
    virtual void draw_drag_preview(const TabDragVisual&,std::string_view title,float scale)=0;
};
std::unique_ptr<BrowserView> make_browser_view(std::string_view style="classic");
}
