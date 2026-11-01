// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "browser_services.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cyurium {
enum class Page { Home, Pending, Settings, Bookmarks, History, Downloads, Extensions };
enum class Theme { Light, Dark };
enum class WebAction { Load, Back, Forward, Reload, Stop };
struct WebCommand { std::uint64_t sequence; WebAction action; std::string url; };
struct WebStatus {
    bool connected=false,loading=false,can_back=false,can_forward=false;
    std::string url,title,error;
    std::string security="unknown",security_origin,security_issuer;
};
struct Destination {
    std::string url;
    std::string title;
    Page page = Page::Home;
};
struct Tab {
    std::uint64_t id;
    std::vector<Destination> history{{"cyurium://newtab", "Nouvel onglet", Page::Home}};
    std::size_t cursor = 0;
    WebStatus web;
    std::vector<WebCommand> web_commands;
    std::uint64_t next_request=1;
    nlohmann::json session_state;
    const Destination& current() const { return history[cursor]; }
    bool can_back() const { return (current().page==Page::Pending&&web.connected&&web.can_back)||cursor>0; }
    bool can_forward() const { return (current().page==Page::Pending&&web.connected&&web.can_forward)||cursor+1<history.size(); }
    void request(WebAction action,std::string url={}) { web_commands.push_back({next_request++,action,std::move(url)}); }
};
struct Bookmark { std::string title, url; };
struct BrowserProfile {
    BrowserServices services;
    std::vector<Bookmark> bookmarks{{"GitHub", "https://github.com"}, {"Wikipedia", "https://fr.wikipedia.org"},
        {"Mozilla", "https://www.mozilla.org"}, {"MDN Web Docs", "https://developer.mozilla.org"}};
    std::vector<std::string> entered_addresses;
    Theme theme = Theme::Dark;
    bool bookmarks_visible = true;
    float ui_scale = 1.0f;
    std::uint64_t next_tab_id = 1;
};
class BrowserModel {
public:
    explicit BrowserModel(std::shared_ptr<BrowserProfile> shared = {}, bool initial_tab = true);
    Tab& active();
    const Tab& active() const;
    void new_tab();
    void select(std::uint64_t id);
    void close(std::uint64_t id);
    bool reorder(std::uint64_t id, std::size_t before);
    bool transfer_to(BrowserModel& target, std::uint64_t id, std::size_t before);
    void cycle(int direction);
    void navigate(std::string_view input);
    void back();
    void forward();
    void reload();
    void stop();
    void web_state(std::uint64_t id,WebStatus status,WebAction operation=WebAction::Load,bool command=false);
    void open(Page page);
    void toggle_bookmark();
    bool bookmarked() const;
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
    std::vector<Tab> tabs;
    std::shared_ptr<BrowserProfile> profile;
    std::uint64_t selected = 0;
    static Destination resolve(std::string_view input);
};
}

