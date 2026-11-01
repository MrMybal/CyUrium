// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "web_engine.hpp"
#include <functional>

namespace cyurium {
// The owned companion supplies CyToolsCore, not another browser/UI runtime.
// Requests are applied only on the native UI thread, between ImGui frames.
class BrowserSdk {
public:
    using WindowAction=std::function<WindowId(WindowId,TabId)>;
    BrowserSdk(std::filesystem::path base,std::filesystem::path profile);
    ~BrowserSdk();
    bool pump(BrowserWorkspace&,WebEngine*,WindowAction create_or_detach);
    bool event(const WebEvent&);
    void stop();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
// Early stdio entry point: no graphics/Gecko, no implicit enablement.
int run_mcp_bridge(const std::filesystem::path& base,const std::filesystem::path& profile);
}
