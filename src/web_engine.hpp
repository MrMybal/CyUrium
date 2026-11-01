// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "browser_view.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <span>
namespace cyurium {
struct WebPlacement { TabId tab; SDL_Window* window; Rect content; bool visible=true,focus_ui=false; };
struct WebEvent {
    nlohmann::json data;
    std::string type,key,url,message,value,title,instance;
    TabId tab=0;std::uint64_t request=0,process=0;
    bool control=false,shift=false,alt=false;
};
class WebEngine {
public:
    WebEngine(std::filesystem::path profile,bool testing=false,bool hardware=true);
    ~WebEngine();
    bool pump(BrowserWorkspace&);
    void sync(BrowserWorkspace&,std::span<const WebPlacement>);
    std::vector<WebEvent> take_events();
    void inspect(TabId,std::uint64_t request);
    void automate(TabId,std::uint64_t request,std::string operation,nlohmann::json parameters);
    bool ready() const;
    std::size_t native_views() const;
    bool embedded(TabId,SDL_Window*) const;
    std::string version() const;
    void restart(BrowserWorkspace&,bool hardware);
    void test_crash();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    unsigned restarts_=0;
};
}
