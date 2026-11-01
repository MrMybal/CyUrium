// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "browser_view.hpp"
#include <span>

namespace cyurium {
struct WindowDropZone { WindowId window; TabStripLayout strip; Rect bounds; };
struct PointerFrame { Point position; bool pressed=false,down=false,released=false,cancel=false; };
struct TabDrop {
    WindowId source=0,target=0;
    TabId tab=0;
    std::size_t before=0;
    Point position;
};
class TabDragController {
public:
    std::optional<TabDrop> update(const PointerFrame&,std::span<const WindowDropZone>);
    void cancel() { visual_={};candidate_=0; }
    const TabDragVisual& visual() const { return visual_; }
    bool tracking() const { return candidate_!=0; }
private:
    TabDragVisual visual_;
    TabId candidate_=0;
    Point pressed_;
};
}
