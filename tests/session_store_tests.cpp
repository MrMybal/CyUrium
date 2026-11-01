// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "session_store.hpp"
#include "session_protection.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
using namespace cyurium;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
    const auto folder=std::filesystem::temp_directory_path()/("cyurium-session-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    BrowserWorkspace workspace;auto first=workspace.create_window();auto& model=*workspace.find(first);
    model.navigate("https://example.org/first");auto old=model.selected;model.new_tab();model.navigate("https://example.org/second");model.navigate("https://example.org/third");model.back();auto tab=model.selected;
    auto second=workspace.detach_tab(first,tab);check(second!=0,"detach failed");
    SessionStore store(folder);if(!store.checkpoint(workspace))throw std::runtime_error(store.error);
    auto bytes=[&](const char* name){std::ifstream f(folder/name,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
#ifdef _WIN32
    check(bytes("session.json").starts_with("CYURIUM-DPAPI-1\n")&&bytes("session.json").find("https://example.org")==std::string::npos,"session was not encrypted");
#endif
    BrowserWorkspace loaded;SessionStore afterCrash(folder);check(afterCrash.restore(loaded)&&afterCrash.recovered(),"crash not detected");
    check(loaded.size()==2&&loaded.owner(old)!=loaded.owner(tab),"window or tab identity lost");
    auto& restored=loaded.find(loaded.owner(tab))->active();
    check(restored.id==tab&&restored.current().url=="https://example.org/second"&&restored.can_forward(),"history or selection lost");
    check(restored.web_commands.size()==1&&restored.web_commands[0].url==restored.current().url,"restoration did not schedule page");
    check(afterCrash.checkpoint(loaded,true),"clean checkpoint failed");
    SessionStore afterExit(folder);BrowserWorkspace clean;check(afterExit.restore(clean)&&!afterExit.recovered(),"clean exit marked as crash");
    bool legacy=false;auto snapshot=nlohmann::json::parse(unprotect_session(bytes("session.json"),legacy));snapshot["version"]=1;
    snapshot["windows"][0]["tabs"][0]["document"]={{"url","https://example.org/first"},{"fields",nlohmann::json::array({{{"id","sensitive"},{"value","must-not-survive-legacy-migration"}}})}};
    {std::ofstream f(folder/"session.json");f<<snapshot.dump();}
    SessionStore migration(folder);BrowserWorkspace migrated;migrated.profile->services.restore_forms=true;
    check(migration.restore(migrated),"legacy migration failed");
    for(auto id:migrated.window_ids())for(auto& t:migrated.find(id)->tabs)check(!t.session_state.is_object()||!t.session_state.contains("fields"),"legacy plaintext form values retained");
    check(bytes("session.json").find("must-not-survive")==std::string::npos&&bytes("session.json.bak").find("must-not-survive")==std::string::npos,"plaintext retained in migrated files");
    {std::ofstream file(folder/"session.json");file<<"broken";}
    SessionStore backup(folder);BrowserWorkspace recovered;check(backup.restore(recovered)&&backup.recovered()&&recovered.size()==2,"backup not recovered");
    {std::ofstream file(folder/"session.json.bak");file<<"{}";}
    auto count=recovered.size();auto next=recovered.profile->next_tab_id;check(!SessionStore(folder).restore(recovered)&&recovered.size()==count&&recovered.profile->next_tab_id==next,"invalid session mutated workspace");
    std::filesystem::remove(folder/"session.json");std::filesystem::remove(folder/"session.json.bak");std::filesystem::remove(folder);
    std::cout<<"PASS: windows, identities, history, account encryption, safe plaintext migration, backup and invalid-session isolation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
