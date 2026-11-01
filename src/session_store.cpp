// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "session_store.hpp"
#include "session_protection.hpp"
#include <fstream>
#include <set>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace cyurium {
using Json=nlohmann::json;
namespace {
bool forms_enabled(const BrowserWorkspace& w){
#ifdef _WIN32
    return w.profile->services.restore_forms;
#else
    (void)w;return false;
#endif
}
void strip_forms(Json& data){for(auto& w:data.at("windows"))for(auto& t:w.at("tabs"))if(t.contains("document")&&t["document"].is_object())t["document"].erase("fields");}
std::string read_bytes(const std::filesystem::path& path){
    if(std::filesystem::file_size(path)>32*1024*1024)throw std::runtime_error("Session trop grande");
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Lecture de session impossible");return {std::istreambuf_iterator<char>(file),{}};
}
void write_bytes(const std::filesystem::path& path,const std::string& bytes){
    auto tmp=path;tmp+=".tmp";
    {std::ofstream file(tmp,std::ios::binary|std::ios::trunc);file.write(bytes.data(),std::streamsize(bytes.size()));file.flush();if(!file)throw std::runtime_error("Écriture de session impossible");}
#ifdef _WIN32
    if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Remplacement de session impossible");
#else
    std::filesystem::rename(tmp,path);
#endif
}
}
bool SessionStore::checkpoint(const BrowserWorkspace& workspace,bool clean) {
    try {
        Json data={{"version",2},{"clean",clean},{"windows",Json::array()}};
        for(auto id:workspace.window_ids()) {
            auto& model=*workspace.find(id);Json window={{"selected",model.selected},{"tabs",Json::array()}};
            if(auto it=workspace.placements.find(id);it!=workspace.placements.end())window["bounds"]=it->second;
            for(auto& tab:model.tabs) {
                Json history=Json::array();
                for(auto& d:tab.history)history.push_back({{"url",d.url},{"title",d.title}});
                window["tabs"].push_back({{"id",tab.id},{"cursor",tab.cursor},{"history",history},{"document",tab.session_state}});
            }
            if(!model.tabs.empty())data["windows"].push_back(std::move(window));
        }
        // Closing the last window preserves its last checkpoint for next launch.
        if(data["windows"].empty()) {if(last_.is_null())return true;data=last_;data["clean"]=clean;}
        if(!forms_enabled(workspace))strip_forms(data);
        if(data==last_)return true;
        std::filesystem::create_directories(path_.parent_path());
        auto backup=path_;backup+=".bak";
        // Never copy legacy plaintext or a corrupt primary file over a good backup.
        if(std::filesystem::exists(path_))try{
            bool legacy=false;auto previous=Json::parse(unprotect_session(read_bytes(path_),legacy));
            if(!previous.is_object()||!previous.contains("windows"))throw std::runtime_error("Invalid backup source");
            if(legacy||!forms_enabled(workspace))strip_forms(previous);
            write_bytes(backup,protect_session(previous.dump()));
        }catch(...){}
        const auto protected_data=protect_session(data.dump());write_bytes(path_,protected_data);
        if(std::filesystem::exists(backup)){
            bool legacy=false;try{unprotect_session(read_bytes(backup),legacy);}catch(...){}
            if(legacy)write_bytes(backup,protected_data);
        }
        last_=std::move(data);error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool SessionStore::restore(BrowserWorkspace& workspace) {
    for(int attempt=0;attempt<2;++attempt)try {
        auto path=path_;if(attempt)path+=".bak";
        if(!std::filesystem::exists(path))continue;
        bool legacy=false;Json data=Json::parse(unprotect_session(read_bytes(path),legacy));
        if((data.at("version")!=1&&data.at("version")!=2)||!data.at("windows").is_array()||data["windows"].empty()||data["windows"].size()>64)throw std::runtime_error("Session invalide");
        if(legacy||!forms_enabled(workspace))strip_forms(data);
        BrowserWorkspace restored;restored.profile=workspace.profile;std::set<TabId> ids;TabId next=1;std::size_t count=0;
        for(auto& w:data["windows"]) {
            if(!w.at("tabs").is_array()||w["tabs"].empty()||w["tabs"].size()>500)throw std::runtime_error("Fenêtre invalide");
            const auto window=restored.create_window(false);auto& model=*restored.find(window);
            if(w.contains("bounds")){const auto& b=w["bounds"];if(!b.is_array()||b.size()!=4)throw std::runtime_error("Position invalide");auto bounds=b.get<std::array<float,4>>();if(!(std::abs(bounds[0])<65536&&std::abs(bounds[1])<65536&&bounds[2]>=720&&bounds[2]<=16384&&bounds[3]>=520&&bounds[3]<=16384))throw std::runtime_error("Dimensions invalides");restored.placements[window]=bounds;}
            for(auto& t:w["tabs"]) {
                if(++count>1000)throw std::runtime_error("Session trop grande");
                Tab tab{t.at("id").get<TabId>()};if(!tab.id||tab.id>1000000000||!ids.insert(tab.id).second)throw std::runtime_error("Identifiant invalide");
                tab.history.clear();const auto& history=t.at("history");
                if(!history.is_array()||history.empty()||history.size()>1000)throw std::runtime_error("Historique invalide");
                for(auto& h:history){auto url=h.at("url").get<std::string>();if(url.size()>32768)throw std::runtime_error("URL trop longue");auto destination=BrowserModel::resolve(url);destination.title=h.value("title",destination.title);tab.history.push_back(std::move(destination));}
                tab.cursor=t.at("cursor").get<std::size_t>();if(tab.cursor>=tab.history.size())throw std::runtime_error("Position invalide");
                tab.session_state=t.value("document",Json());if(!tab.session_state.is_null()&&(!tab.session_state.is_object()||tab.session_state.dump().size()>131072))throw std::runtime_error("État de document invalide");
                if(tab.current().page==Page::Pending)tab.request(WebAction::Load,tab.current().url);
                next=std::max(next,tab.id+1);model.tabs.push_back(std::move(tab));
            }
            model.selected=w.at("selected").get<TabId>();if(std::none_of(model.tabs.begin(),model.tabs.end(),[&](auto& t){return t.id==model.selected;}))throw std::runtime_error("Sélection invalide");
        }
        if(legacy){last_=nullptr;if(!checkpoint(restored,data.value("clean",false)))throw std::runtime_error(error);}
        workspace=std::move(restored);workspace.profile->next_tab_id=next;
        if(!legacy)last_=data;recovered_=!data.value("clean",false)||attempt!=0;error.clear();
        return true;
    }catch(const std::exception& e){error=e.what();}
    return false;
}
}
