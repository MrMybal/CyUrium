// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "browser_model.hpp"
#include <map>
#include <array>

namespace cyurium {
using WindowId = std::uint64_t;
using TabId = std::uint64_t;

// Owns browser windows and tabs; it knows nothing about SDL or a visual layout.
class BrowserWorkspace {
public:
    std::shared_ptr<BrowserProfile> profile = std::make_shared<BrowserProfile>();
    std::map<WindowId,std::array<float,4>> placements;
    WindowId create_window(bool initial_tab = true);
    BrowserModel* find(WindowId id);
    const BrowserModel* find(WindowId id) const;
    WindowId owner(TabId tab) const;
    bool close_window(WindowId id);
    bool close_tab(WindowId window, TabId tab);
    bool move_tab(WindowId source, WindowId target, TabId tab, std::size_t before);
    WindowId detach_tab(WindowId source, TabId tab);
    std::vector<WindowId> window_ids() const;
    std::size_t size() const { return windows_.size(); }
private:
    std::map<WindowId, std::unique_ptr<BrowserModel>> windows_;
    WindowId next_window_ = 1;
};
}
