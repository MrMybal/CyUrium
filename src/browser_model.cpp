// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_model.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <system_error>
#include <stdexcept>

namespace cyurium {
namespace {
std::string trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return std::string(value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1));
}
std::string encode(std::string_view text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') result += char(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}
}
BrowserModel::BrowserModel(std::shared_ptr<BrowserProfile> shared,bool initial_tab)
    : profile(shared?std::move(shared):std::make_shared<BrowserProfile>()) { if(initial_tab)new_tab(); }
Tab& BrowserModel::active() {
    if(tabs.empty())throw std::logic_error("No active tab in an empty browser window");
    return *std::find_if(tabs.begin(), tabs.end(), [&](const Tab& tab) { return tab.id == selected; });
}
const Tab& BrowserModel::active() const {
    if(tabs.empty())throw std::logic_error("No active tab in an empty browser window");
    return *std::find_if(tabs.begin(), tabs.end(), [&](const Tab& tab) { return tab.id == selected; });
}
void BrowserModel::new_tab() { selected = profile->next_tab_id++; tabs.push_back(Tab{selected}); }
void BrowserModel::select(std::uint64_t id) {
    if (std::any_of(tabs.begin(), tabs.end(), [&](const Tab& tab) { return tab.id == id; })) selected = id;
}
void BrowserModel::close(std::uint64_t id) {
    const auto it = std::find_if(tabs.begin(), tabs.end(), [&](const Tab& tab) { return tab.id == id; });
    if (it == tabs.end()) return;
    const auto index = static_cast<std::size_t>(it - tabs.begin());
    tabs.erase(it);
    if (tabs.empty()) { selected=0; return; }
    if (selected == id) selected = tabs[std::min(index, tabs.size() - 1)].id;
}
bool BrowserModel::reorder(std::uint64_t id,std::size_t before) {
    auto it=std::find_if(tabs.begin(),tabs.end(),[&](const Tab& t){return t.id==id;});
    if(it==tabs.end())return false;
    auto old=std::size_t(it-tabs.begin());before=std::min(before,tabs.size());
    if(before>old)--before;
    if(before==old)return true;
    Tab moved=std::move(*it);tabs.erase(it);tabs.insert(tabs.begin()+before,std::move(moved));return true;
}
bool BrowserModel::transfer_to(BrowserModel& target,std::uint64_t id,std::size_t before) {
    if(&target==this)return reorder(id,before);
    if(profile!=target.profile)return false;
    auto it=std::find_if(tabs.begin(),tabs.end(),[&](const Tab& t){return t.id==id;});
    if(it==tabs.end())return false;
    target.tabs.reserve(target.tabs.size()+1);
    auto old=std::size_t(it-tabs.begin());Tab moved=std::move(*it);tabs.erase(it);
    if(tabs.empty())selected=0;else if(selected==id)selected=tabs[std::min(old,tabs.size()-1)].id;
    before=std::min(before,target.tabs.size());target.tabs.insert(target.tabs.begin()+before,std::move(moved));
    target.selected=id;return true;
}
void BrowserModel::cycle(int direction) {
    auto it = std::find_if(tabs.begin(), tabs.end(), [&](const Tab& tab) { return tab.id == selected; });
    const auto index = static_cast<int>(it - tabs.begin());
    selected = tabs[(index + direction + int(tabs.size())) % int(tabs.size())].id;
}
Destination BrowserModel::resolve(std::string_view input) {
    std::string value = trim(input);
    if (value.empty() || value == "cyurium://newtab") return {"cyurium://newtab", "Nouvel onglet", Page::Home};
    const std::pair<std::string_view, Page> internal[] = {
        {"settings", Page::Settings}, {"bookmarks", Page::Bookmarks}, {"history", Page::History},
        {"downloads", Page::Downloads}, {"extensions", Page::Extensions}};
    for (const auto& [name, page] : internal) {
        if (value == "cyurium://" + std::string(name)) {
            const char* title = page == Page::Settings ? "Paramètres" : page == Page::Bookmarks ? "Favoris" :
                page == Page::History ? "Adresses saisies" : page == Page::Downloads ? "Téléchargements" : "Extensions";
            return {value, title, page};
        }
    }
    // No URL is actually fetched before the desktop engine is connected.
    const bool scheme = value.find("://") != std::string::npos;
    if (!scheme && value.find_first_of(" \t") == std::string::npos &&
        (value.find('.') != std::string::npos || value.starts_with("localhost"))) value = "https://" + value;
    else if (!scheme) value = "https://www.google.com/search?q=" + encode(value);
    const auto start = value.find("://") + 3;
    const auto end = value.find_first_of("/?#", start);
    return {value, value.substr(start, end - start), Page::Pending};
}
void BrowserModel::navigate(std::string_view input) {
    Destination destination = resolve(input);
    if(destination.page==Page::Settings||destination.page==Page::Bookmarks||destination.page==Page::History||
       destination.page==Page::Downloads||destination.page==Page::Extensions){
        // Browser management belongs to its own tab. In particular, do not hide
        // a live Gecko document (and its media) behind an internal destination
        // sharing the same tab ID.
        new_tab();
        auto& internal=active();internal.history={std::move(destination)};internal.cursor=0;
        return;
    }
    auto& tab = active();
    if (tab.current().url == destination.url) return;
    tab.history.erase(tab.history.begin() + static_cast<std::ptrdiff_t>(tab.cursor + 1), tab.history.end());
    tab.history.push_back(destination);
    tab.cursor = tab.history.size() - 1;
    if (destination.page == Page::Pending) {
        tab.web.loading=true;tab.web.error.clear();tab.request(WebAction::Load,destination.url);
        std::erase(profile->entered_addresses, destination.url);
        profile->entered_addresses.insert(profile->entered_addresses.begin(), destination.url);
        if (profile->entered_addresses.size() > 200) profile->entered_addresses.resize(200);
    }
}
void BrowserModel::back() {
    auto& tab=active();
    if(tab.current().page==Page::Pending&&tab.web.connected&&tab.web.can_back){tab.request(WebAction::Back);return;}
    if(tab.cursor){--tab.cursor;if(tab.current().page==Page::Pending&&tab.current().url!=tab.web.url)tab.request(WebAction::Load,tab.current().url);}
}
void BrowserModel::forward() {
    auto& tab=active();
    if(tab.current().page==Page::Pending&&tab.web.connected&&tab.web.can_forward){tab.request(WebAction::Forward);return;}
    if(tab.cursor+1<tab.history.size()){++tab.cursor;if(tab.current().page==Page::Pending&&tab.current().url!=tab.web.url)tab.request(WebAction::Load,tab.current().url);}
}
void BrowserModel::reload(){if(active().current().page==Page::Pending)active().request(WebAction::Reload);}
void BrowserModel::stop(){if(active().current().page==Page::Pending)active().request(WebAction::Stop);}
void BrowserModel::web_state(std::uint64_t id,WebStatus status,WebAction operation,bool command) {
    auto it=std::find_if(tabs.begin(),tabs.end(),[&](const Tab& tab){return tab.id==id;});if(it==tabs.end())return;
    auto& tab=*it;tab.web=std::move(status);
    if(tab.current().page!=Page::Pending||tab.web.url.empty()||tab.web.url=="about:blank")return;
    if(tab.current().url!=tab.web.url) {
        std::optional<std::size_t> index;
        if(command&&operation==WebAction::Back)for(std::size_t i=tab.cursor;i>0;--i)if(tab.history[i-1].url==tab.web.url){index=i-1;break;}
        if(command&&operation==WebAction::Forward)for(std::size_t i=tab.cursor+1;i<tab.history.size();++i)if(tab.history[i].url==tab.web.url){index=i;break;}
        if(index)tab.cursor=*index;
        else if(command&&operation==WebAction::Load)tab.history[tab.cursor].url=tab.web.url;
        else {tab.history.erase(tab.history.begin()+tab.cursor+1,tab.history.end());tab.history.push_back({tab.web.url,tab.web.title,Page::Pending});++tab.cursor;}
    }
    if(!tab.web.title.empty())tab.history[tab.cursor].title=tab.web.title;
}
void BrowserModel::open(Page page) {
    const char* name = page == Page::Settings ? "settings" : page == Page::Bookmarks ? "bookmarks" :
        page == Page::History ? "history" : page == Page::Downloads ? "downloads" :
        page == Page::Extensions ? "extensions" : "newtab";
    navigate("cyurium://" + std::string(name));
}
bool BrowserModel::bookmarked() const {
    return std::any_of(profile->bookmarks.begin(), profile->bookmarks.end(), [&](const Bookmark& b) { return b.url == active().current().url; });
}
void BrowserModel::toggle_bookmark() {
    if (active().current().page != Page::Pending) return;
    if (bookmarked()) std::erase_if(profile->bookmarks, [&](const Bookmark& b) { return b.url == active().current().url; });
    else profile->bookmarks.push_back({active().current().title, active().current().url});
}
bool BrowserModel::save(const std::filesystem::path& path) const {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    auto temporary = path; temporary += ".tmp";
    std::ofstream out(temporary, std::ios::trunc);
    out << "CYURIUM 1\n" << int(profile->theme) << ' ' << profile->bookmarks_visible << ' ' << profile->ui_scale << '\n';
    out << "S " << profile->services.hardware_acceleration << ' ' << profile->services.restore_session << '\n';
    out << "P " << profile->services.restore_forms << '\n';
    out << "M " << profile->services.sdk_enabled << ' ' << profile->services.sdk_control << '\n';
    for (const auto& b : profile->bookmarks) out << "B " << std::quoted(b.title) << ' ' << std::quoted(b.url) << '\n';
    for (const auto& url : profile->entered_addresses) out << "H " << std::quoted(url) << '\n';
    out.close();
    if (!out) return false;
    // Rename the old file aside first; never discard the last valid settings.
    auto backup = path; backup += ".bak";
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(backup, error); error.clear();
        std::filesystem::rename(path, backup, error);
        if (error) return false;
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code rollback;
        if (std::filesystem::exists(backup)) std::filesystem::rename(backup, path, rollback);
        return false;
    }
    return true;
}
bool BrowserModel::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::string magic; int version{}, theme_value{}; bool show{}; float scale{};
    if (!(in >> magic >> version) || magic != "CYURIUM" || version != 1 ||
        !(in >> theme_value >> show >> scale) || theme_value < 0 || theme_value > 1 ||
        !(scale >= .85f && scale <= 1.5f)) return false;
    std::vector<Bookmark> loaded_bookmarks;
    std::vector<std::string> loaded_history;
    std::string kind;
    bool acceleration=true,restore=true,forms=false,sdk=false,control=false;
    while (in >> kind) {
        std::string a, b;
        if (kind == "S" && in >> acceleration >> restore) {}
        else if (kind == "P" && in >> forms) {}
        else if (kind == "M" && in >> sdk >> control) {}
        else if (kind == "B" && in >> std::quoted(a) >> std::quoted(b)) loaded_bookmarks.push_back({a, b});
        else if (kind == "H" && in >> std::quoted(a)) loaded_history.push_back(a);
        else return false;
        if (loaded_bookmarks.size() > 1000 || loaded_history.size() > 200) return false;
    }
    if (!in.eof()) return false;
    profile->theme = static_cast<Theme>(theme_value); profile->bookmarks_visible = show; profile->ui_scale = scale;
    profile->services.hardware_acceleration=acceleration;profile->services.restore_session=restore;
    profile->services.restore_forms=forms;
    profile->services.sdk_enabled=sdk;profile->services.sdk_control=control;
    profile->bookmarks = std::move(loaded_bookmarks); profile->entered_addresses = std::move(loaded_history);
    return true;
}
}

