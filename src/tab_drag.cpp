// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "tab_drag.hpp"
#include <algorithm>

namespace cyurium {
std::size_t TabStripLayout::insertion(Point p) const {
    for(const auto& slot:slots)if(p.x<slot.body.x+slot.body.w*.5f)return slot.index;
    return slots.empty()?total:slots.back().index+1;
}
float TabStripLayout::marker(std::size_t before) const {
    for(const auto& slot:slots)if(slot.index>=before)return slot.body.x;
    return slots.empty()?drop_area.x:slots.back().body.x+slots.back().body.w;
}
std::optional<TabDrop> TabDragController::update(const PointerFrame& pointer,std::span<const WindowDropZone> zones) {
    if(pointer.cancel){cancel();return {};}
    if(pointer.pressed) {
        cancel();
        // Zones are supplied frontmost first by the native window host.
        for(const auto& zone:zones) {
            if(!zone.strip.drop_area.contains(pointer.position)) {
                if(zone.bounds.contains(pointer.position))break;
                continue;
            }
            for(const auto& slot:zone.strip.slots)if(slot.body.contains(pointer.position)&&!slot.close.contains(pointer.position)) {
                candidate_=slot.id;pressed_=pointer.position;
                visual_.tab=slot.id;visual_.source=zone.window;
                visual_.grab_offset={pointer.position.x-slot.body.x,pointer.position.y-slot.body.y};
                break;
            }
            break;
        }
    }
    if(!candidate_)return {};
    const bool exists=std::any_of(zones.begin(),zones.end(),[&](const auto& z){return z.window==visual_.source;});
    if(!exists){cancel();return {};}
    const float dx=pointer.position.x-pressed_.x,dy=pointer.position.y-pressed_.y;
    if(pointer.down&&dx*dx+dy*dy>=36)visual_.active=true;
    visual_.pointer=pointer.position;visual_.target=0;
    if(visual_.active)for(const auto& zone:zones) {
        if(zone.strip.drop_area.contains(pointer.position)) {
            visual_.target=zone.window;visual_.before=zone.strip.insertion(pointer.position);break;
        }
        if(zone.bounds.contains(pointer.position))break;
    }
    if(pointer.released) {
        std::optional<TabDrop> result;
        if(visual_.active)result=TabDrop{visual_.source,visual_.target,candidate_,visual_.before,
            {pointer.position.x-visual_.grab_offset.x,pointer.position.y-visual_.grab_offset.y}};
        cancel();return result;
    }
    if(!pointer.down){cancel();}
    return {};
}
}
