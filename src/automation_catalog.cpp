// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "automation.hpp"
#include "automation_catalog_data.hpp"
#include "browser_model.hpp"
#include <cstdlib>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif
namespace cyurium::automation {
const Json& operation_catalog(){static const Json catalogue=Json::parse(cyurium_operations_json);return catalogue;}
bool read_only(const std::string& name){for(auto& op:operation_catalog())if(op["name"]==name)return op["readOnly"].get<bool>();return false;}
void register_operations(Registry& registry,Invoke invoke){
    for(const auto& item:operation_catalog()){
        Operation op;op.name=item["name"];op.description=item["description"];op.input_schema=item["inputSchema"];op.output_schema=item["outputSchema"];op.read_only=item["readOnly"];
        op.handler=[invoke,name=op.name](const Json& args,const CallContext& context){return invoke(name,args,context);};registry.add(std::move(op));
    }
}
std::filesystem::path default_profile(){
#ifdef _WIN32
    wchar_t folder[MAX_PATH];if(FAILED(SHGetFolderPathW(nullptr,CSIDL_APPDATA,nullptr,0,folder)))throw Error("FileError","Cannot locate application profile");
    return std::filesystem::path(folder)/"CyUrium/CyUrium";
#else
    const char* xdg=std::getenv("XDG_DATA_HOME");const char* home=std::getenv("HOME");
#ifdef __APPLE__
    return std::filesystem::path(home?home:".")/"Library/Application Support/CyUrium/CyUrium";
#else
    return (xdg?std::filesystem::path(xdg):std::filesystem::path(home?home:".")/".local/share")/"CyUrium/CyUrium";
#endif
#endif
}
Json read_profile(const std::filesystem::path& profile,const std::string& operation,const Json& args){
    BrowserModel model({},false);if(!model.load(profile/"settings.txt"))throw Error("NoProfile","No readable saved profile at the selected location");
    if(!model.profile->services.sdk_enabled)throw Error("AccessDenied","External automation is disabled in this profile");
    if(operation=="settings_get"||operation=="profile_info")return {{"theme",model.profile->theme==Theme::Dark?"dark":"light"},{"uiScale",model.profile->ui_scale},{"bookmarksVisible",model.profile->bookmarks_visible},{"hardwareAcceleration",model.profile->services.hardware_acceleration},{"savedProfile",true}};
    Json rows=Json::array();std::string query=args.value("query","");
    if(operation=="bookmarks_list"){for(auto& b:model.profile->bookmarks)if(query.empty()||b.title.find(query)!=b.title.npos||b.url.find(query)!=b.url.npos)rows.push_back({{"title",b.title},{"url",b.url}});}
    else if(operation=="history_list"){for(auto& u:model.profile->entered_addresses)if(query.empty()||u.find(query)!=u.npos)rows.push_back({{"url",u}});}
    else throw Error("InstanceRequired","This operation requires an open browser instance");
    auto offset=std::min(args.value("offset",std::size_t(0)),rows.size()),end=std::min(rows.size(),offset+args.value("limit",std::size_t(50)));Json page=Json::array();for(auto i=offset;i<end;++i)page.push_back(rows[i]);
    return {{"items",page},{"total",rows.size()},{"offset",offset},{"nextOffset",end<rows.size()?Json(end):Json()}};
}
}
