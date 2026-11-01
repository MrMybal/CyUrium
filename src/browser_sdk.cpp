// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_sdk.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace cyurium {
namespace {
using Json=nlohmann::json;
std::string utf8(const std::filesystem::path& p){auto s=p.u8string();return {s.begin(),s.end()};}
bool web_url(std::string_view u){
    if(!(u.starts_with("https://")||u.starts_with("http://"))||u.size()>8192||std::any_of(u.begin(),u.end(),[](unsigned char c){return c<=32||c==127;}))return false;
    auto start=u.find("://")+3,end=u.find_first_of("/?#",start);auto authority=u.substr(start,end==u.npos?u.size()-start:end-start);
    return !authority.empty()&&authority.find('@')==authority.npos;
}
SDL_Process* launch(const std::filesystem::path& base,const std::filesystem::path& profile,bool control,bool mcp,SDL_IOStream* log=nullptr,bool external=false){
    auto python=base/"sdk/python/python.exe";
#ifndef _WIN32
    python=base/"sdk/python/bin/python3";
#endif
    std::vector<std::string> args={utf8(python),"-I","-B","-u",utf8(base/"sdk"/(mcp?"client.py":"service.py")),"--profile",utf8(profile)};
    if(control&&!mcp)args.push_back("--control");
    if(external&&!mcp)args.push_back("--external");
    std::vector<const char*> pointers;for(auto& arg:args)pointers.push_back(arg.c_str());pointers.push_back(nullptr);
    auto props=SDL_CreateProperties();
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,pointers.data());
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,mcp?SDL_PROCESS_STDIO_INHERITED:SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,mcp?SDL_PROCESS_STDIO_INHERITED:SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,log?SDL_PROCESS_STDIO_REDIRECT:SDL_PROCESS_STDIO_INHERITED);
    if(log)SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_POINTER,log);
    auto* process=SDL_CreateProcessWithProperties(props);SDL_DestroyProperties(props);return process;
}
}
struct BrowserSdk::Impl {
    std::filesystem::path base,profile;
    SDL_Process* process=nullptr;SDL_IOStream* log=nullptr;
    std::thread reader;std::mutex mutex;std::vector<Json> incoming;
    std::atomic<bool> stopping=false,failed=false;
    bool enabled=false,external=false,control=false;std::string error;
    std::map<std::uint64_t,std::chrono::steady_clock::time_point> pending;
#ifdef _WIN32
    HANDLE job=nullptr;
#endif
    void send(Json data){
        if(!process)return;
        auto line=data.dump()+"\n";auto* pipe=SDL_GetProcessInput(process);
        if(!pipe||SDL_WriteIO(pipe,line.data(),line.size())!=line.size()||!SDL_FlushIO(pipe))failed=true;
    }
    void reply(std::uint64_t id,Json data){send({{"id",id},{"result",std::move(data)}});}
    void reject(std::uint64_t id,std::string message,std::string code="AccessDenied"){send({{"id",id},{"error",{{"code",std::move(code)},{"message",std::move(message)}}}});}
    void start(){
        std::filesystem::create_directories(profile/"sdk");
        log=SDL_IOFromFile(utf8(profile/"sdk/runtime.log").c_str(),"ab");
        process=launch(base,profile,control,false,log,external);
        if(!process)throw std::runtime_error(SDL_GetError());
#ifdef _WIN32
        job=CreateJobObjectW(nullptr,nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        auto pid=DWORD(SDL_GetNumberProperty(SDL_GetProcessProperties(process),SDL_PROP_PROCESS_PID_NUMBER,0));
        HANDLE child=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,pid);
        bool owned=job&&child&&SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))&&AssignProcessToJobObject(job,child);
        if(child)CloseHandle(child);
        if(!owned)throw std::runtime_error("Impossible de confiner le compagnon SDK à la durée de vie du navigateur.");
#endif
        stopping=false;failed=false;
        reader=std::thread([this]{
            std::string buffer;char bytes[4096];auto* pipe=SDL_GetProcessOutput(process);
            while(!stopping){
                auto count=SDL_ReadIO(pipe,bytes,sizeof(bytes));
                if(!count){if(SDL_GetIOStatus(pipe)==SDL_IO_STATUS_NOT_READY){SDL_Delay(10);continue;}break;}
                buffer.append(bytes,count);
                if(buffer.size()>4*1024*1024){failed=true;break;}
                for(auto end=buffer.find('\n');end!=buffer.npos;end=buffer.find('\n')){
                    auto value=Json::parse(buffer.substr(0,end),nullptr,false);buffer.erase(0,end+1);
                    if(value.is_discarded()||!value.is_object()){failed=true;break;}
                    {std::lock_guard lock(mutex);if(incoming.size()>=16){failed=true;break;}incoming.push_back(std::move(value));}
                    SDL_Event event{};event.type=SDL_EVENT_USER;SDL_PushEvent(&event);
                }
                if(failed)break;
            }
            if(!stopping)failed=true;
        });
    }
    void stop(){
        if(process){send({{"type","shutdown"}});stopping=true;SDL_KillProcess(process,true);if(reader.joinable())reader.join();SDL_WaitProcess(process,true,nullptr);SDL_DestroyProcess(process);process=nullptr;}
#ifdef _WIN32
        if(job){CloseHandle(job);job=nullptr;}
#endif
        if(log){SDL_CloseIO(log);log=nullptr;}
        pending.clear();std::lock_guard lock(mutex);incoming.clear();
    }
};
BrowserSdk::BrowserSdk(std::filesystem::path base,std::filesystem::path profile):impl_(std::make_unique<Impl>()){impl_->base=std::move(base);impl_->profile=std::move(profile);}
BrowserSdk::~BrowserSdk(){stop();}
void BrowserSdk::stop(){impl_->stop();}
bool BrowserSdk::event(const WebEvent& event){
    if(event.type!="automation-result")return false;
    auto& p=*impl_;if(!p.pending.erase(event.request))return true;
    if(event.message.empty())p.reply(event.request,event.data);else p.reject(event.request,event.message,"PageError");return true;
}
bool BrowserSdk::pump(BrowserWorkspace& workspace,WebEngine* engine,WindowAction window){
    auto& p=*impl_;auto& services=workspace.profile->services;bool changed=false;
    if(services.assistant_visible)services.assistant_started=true;
    const bool wanted=services.sdk_enabled||services.assistant_started;
    if(p.enabled!=wanted||p.external!=services.sdk_enabled||p.control!=services.sdk_control){
        p.stop();p.enabled=wanted;p.external=services.sdk_enabled;p.control=services.sdk_control;p.error.clear();changed=true;
        services.state["sdk"]={{"ready",false},{"message",p.enabled?"Démarrage de CyToolsCore…":"MCP / SDK désactivé"}};
        if(p.enabled)try{p.start();}catch(const std::exception& e){p.stop();p.error=e.what();}
    }
    if(p.failed){p.stop();p.error="Le compagnon SDK s’est arrêté. Désactivez puis réactivez le service pour le relancer.";p.failed=false;changed=true;}
    if(!p.error.empty()){services.state["sdk"]={{"ready",false},{"message",p.error}};return changed;}
    if((!engine||!engine->ready())&&!p.pending.empty()){
        for(auto& [id,_]:p.pending)p.reject(id,"Le processus de page s’est arrêté pendant la requête.","Disconnected");
        p.pending.clear();changed=true;
    }
    std::vector<Json> requests;{std::lock_guard lock(p.mutex);requests.swap(p.incoming);}
    if(p.process){for(auto& command:services.assistant_commands)p.send({{"type","assistant"},{"command",std::move(command)}});services.assistant_commands.clear();}
    for(const auto& request:requests){changed=true;std::uint64_t id=0;try{
        const auto type=request.value("type","");
        if(type=="assistant-state"){services.assistant=request.at("data");continue;}
        if(type=="assistant-delta"){
            auto chat=request.value("conversationId","");for(auto& item:services.assistant["conversations"])if(item.value("id","")==chat){item["partial"]=item.value("partial","")+request.value("text","");}continue;
        }
        if(type=="ready"){
            services.state["sdk"]={{"ready",p.external},{"message",!p.external?"MCP externe désactivé · assistant natif actif":p.control?"CyToolsCore 0.10.1 · lecture et contrôle autorisés":"CyToolsCore 0.10.1 · lecture seule"}};
            const auto executable=p.base/
#ifdef _WIN32
                "CyUrium.exe";
#else
                "CyUrium";
#endif
            if(p.external)services.state["sdk"]["config"]={{"mcpServers",{{"CyUrium",{{"command",utf8(executable)},{"args",Json::array({"--mcp","--profile",utf8(p.profile)})}}}}}};
            continue;
        }
        id=request.at("id").get<std::uint64_t>();if(!id)throw std::runtime_error("Invalid request ID");
        if(type=="cancel"){p.pending.erase(id);continue;}
        if(type!="request")throw std::runtime_error("Invalid native request");
        const auto operation=request.at("operation").get<std::string>();const auto& args=request.at("parameters");
        if((!services.sdk_enabled&&!request.value("internal",false))||(!services.sdk_control&&operation!="browser_state"&&operation!="page_snapshot")){p.reject(id,"Le contrôle MCP est désactivé dans les paramètres.");continue;}
        auto target=args.value("tabId",TabId(0));auto owner=workspace.owner(target);auto* model=workspace.find(owner);
        if(operation=="browser_state"){
            Json windows=Json::array();for(auto wid:workspace.window_ids()){
                auto* m=workspace.find(wid);Json tabs=Json::array();for(auto& tab:m->tabs)tabs.push_back({{"tabId",tab.id},{"url",tab.current().url},{"title",tab.current().title},{"loading",tab.web.loading},{"security",tab.web.security}});
                windows.push_back({{"windowId",wid},{"selectedTab",m->selected},{"tabs",std::move(tabs)}});
            }p.reply(id,{{"windows",windows}});continue;
        }
        if(operation=="window_new"){p.reply(id,{{"windowId",window(0,0)}});continue;}
        if(operation=="window_close"){
            auto wid=args.at("windowId").get<WindowId>();if(workspace.size()<2||!workspace.find(wid))throw std::runtime_error("Cannot close the last or an unknown window");
            workspace.close_window(wid);p.reply(id,{{"closed",true}});continue;
        }
        if(operation=="tab_open"){
            model=workspace.find(args.at("windowId").get<WindowId>());if(!model)throw std::runtime_error("Unknown window");
            if(args.contains("url")&&!web_url(args.at("url").get<std::string>()))throw std::runtime_error("HTTP(S) URL required");
            model->new_tab();if(args.contains("url"))model->navigate(args.at("url").get<std::string>());
            p.reply(id,{{"tabId",model->selected}});continue;
        }
        if(!model)throw std::runtime_error("Unknown tab");
        auto found=std::find_if(model->tabs.begin(),model->tabs.end(),[&](auto& tab){return tab.id==target;});
        if(operation.starts_with("page_")){
            if(!engine||!engine->ready()||found->current().page!=Page::Pending||found->web.loading||!found->web.connected)throw std::runtime_error("Page is not ready");
            const auto url=args.at("url").get<std::string>();if(!web_url(url)||found->web.url!=url)throw std::runtime_error("Page URL changed; take a new snapshot");
            if(p.pending.size()>=16)throw std::runtime_error("Too many pending page operations");
            p.pending[id]=std::chrono::steady_clock::now();engine->automate(target,id,operation,args);continue;
        }
        if(operation=="tab_select")model->select(target);
        else if(operation=="tab_close"){
            if(workspace.size()==1&&model->tabs.size()==1)model->new_tab();
            workspace.close_tab(owner,target);
        }
        else if(operation=="tab_detach"){p.reply(id,{{"windowId",window(owner,target)}});continue;}
        else if(operation=="tab_move"){
            auto destination=args.at("windowId").get<WindowId>();if(!workspace.move_tab(owner,destination,target,args.value("before",std::size_t(0))))throw std::runtime_error("Cannot move tab");
        }else if(operation=="tab_navigate"){
            auto url=args.at("url").get<std::string>();if(!web_url(url))throw std::runtime_error("HTTP(S) URL required");
            auto selected=model->selected;model->select(target);model->navigate(url);model->select(selected);
        }else if(operation=="tab_history"){
            auto selected=model->selected;model->select(target);auto action=args.at("action").get<std::string>();
            if(action=="back")model->back();else if(action=="forward")model->forward();else if(action=="reload")model->reload();else if(action=="stop")model->stop();else throw std::runtime_error("Invalid history action");model->select(selected);
        }else throw std::runtime_error("Unknown browser operation");
        p.reply(id,{{"accepted",true}});
    }catch(const std::exception& error){p.reject(id,error.what(),"InvalidState");}}
    return changed;
}
int run_mcp_bridge(const std::filesystem::path& base,const std::filesystem::path& profile){
    auto* process=launch(base,profile,false,true);if(!process)return 1;
    int code=1;SDL_WaitProcess(process,true,&code);SDL_DestroyProcess(process);return code;
}
}
