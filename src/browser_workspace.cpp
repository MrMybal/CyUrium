// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_workspace.hpp"
#include <algorithm>

namespace cyurium {
WindowId BrowserWorkspace::create_window(bool initial_tab) {
    const auto id = next_window_++;
    windows_.emplace(id,std::make_unique<BrowserModel>(profile,initial_tab));
    return id;
}
BrowserModel* BrowserWorkspace::find(WindowId id) {
    const auto it=windows_.find(id);return it==windows_.end()?nullptr:it->second.get();
}
const BrowserModel* BrowserWorkspace::find(WindowId id) const {
    const auto it=windows_.find(id);return it==windows_.end()?nullptr:it->second.get();
}
WindowId BrowserWorkspace::owner(TabId tab) const {
    for(const auto& [id,window]:windows_)
        if(std::any_of(window->tabs.begin(),window->tabs.end(),[&](const Tab& t){return t.id==tab;}))return id;
    return 0;
}
bool BrowserWorkspace::close_window(WindowId id) { placements.erase(id);return windows_.erase(id)!=0; }
bool BrowserWorkspace::close_tab(WindowId window,TabId tab) {
    auto* model=find(window);if(!model)return false;
    const auto size=model->tabs.size();model->close(tab);
    const bool changed=size!=model->tabs.size();
    if(model->tabs.empty())close_window(window);
    return changed;
}
bool BrowserWorkspace::move_tab(WindowId source,WindowId target,TabId tab,std::size_t before) {
    auto* from=find(source);auto* to=find(target);
    if(!from||!to)return false;
    if(from==to)return from->reorder(tab,before);
    if(!from->transfer_to(*to,tab,before))return false;
    if(from->tabs.empty())close_window(source);
    return true;
}
WindowId BrowserWorkspace::detach_tab(WindowId source,TabId tab) {
    if(owner(tab)!=source)return 0;
    const auto target=create_window(false);
    if(!move_tab(source,target,tab,0)){close_window(target);return 0;}
    return target;
}
std::vector<WindowId> BrowserWorkspace::window_ids() const {
    std::vector<WindowId> result;for(const auto& [id,_]:windows_)result.push_back(id);return result;
}
}
