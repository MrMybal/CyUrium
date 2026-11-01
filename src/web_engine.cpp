// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "web_engine.hpp"
#include "json.hpp"
#ifdef _WIN32
#include "bridge_channel.hpp"
#endif
#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif
namespace cyurium {
using Json=nlohmann::json;
namespace {
std::filesystem::path base_path(){const auto* p=SDL_GetBasePath();return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(p)));}
std::string action_name(WebAction action){switch(action){case WebAction::Back:return "back";case WebAction::Forward:return "forward";case WebAction::Reload:return "reload";case WebAction::Stop:return "stop";default:return "load";}}
WebAction action_value(const std::string& action){if(action=="back")return WebAction::Back;if(action=="forward")return WebAction::Forward;if(action=="reload")return WebAction::Reload;if(action=="stop")return WebAction::Stop;return WebAction::Load;}
#ifdef _WIN32
std::wstring wide(std::string_view s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);std::wstring result(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),result.data(),n);return result;}
std::wstring quote(std::wstring_view s){std::wstring result=L"\"";std::size_t slashes=0;for(auto c:s){if(c==L'\\'){++slashes;continue;}if(c==L'\"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}result.append(slashes*2,L'\\');return result+L'\"';}
HWND hwnd(SDL_Window* window){return static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));}
HWND find_window(std::wstring_view title,DWORD process_id){
    struct Search { std::wstring_view title; DWORD pid; HWND result=nullptr; } search{title,process_id};
    EnumWindows([](HWND window,LPARAM data)->BOOL{
        auto& value=*reinterpret_cast<Search*>(data);wchar_t text[1024]{};GetWindowTextW(window,text,1024);
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(pid==value.pid&&std::wstring_view(text)==value.title){value.result=window;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&search));return search.result;
}
#endif
}
struct WebEngine::Impl {
    std::filesystem::path profile;
    bool testing=false,started=false,connected=false,failed=false,hardware=true;
    std::chrono::steady_clock::time_point failed_at;
    std::string error,platform_version,window_id;
    Json layout;
    std::optional<bool> form_policy;
    std::set<TabId> known;
    std::vector<WebEvent> events;
    std::chrono::steady_clock::time_point launched;
#ifdef _WIN32
    std::unique_ptr<LocalBridge> bridge;
    DWORD process_id=0;
    HANDLE job=nullptr,process=nullptr,log=nullptr;
    bool manager_hidden=false;
    HICON app_icon=nullptr;
    struct Native { HWND window=nullptr,parent=nullptr; Rect bounds; bool visible=false; std::wstring title; std::optional<bool> active; };
    std::map<TabId,Native> native;
    std::mutex mutex;
    std::vector<Json> incoming;
    std::string worker_error;
    void post(Json message){if(testing){auto trace=message;trace.erase("token");trace.erase("proof");std::ofstream(profile/"bridge-events.log",std::ios::app)<<trace.dump()<<'\n';}std::lock_guard guard(mutex);if(incoming.size()>=2048){incoming.clear();message={{"type","disconnected"}};}incoming.push_back(std::move(message));SDL_Event event{};event.type=SDL_EVENT_USER;SDL_PushEvent(&event);}
    void send(Json message){if(!started||failed||!bridge)return;try{bridge->send(std::move(message));}catch(const std::exception& e){fail(e.what());}}
    void start(){
        started=true;launched=std::chrono::steady_clock::now();
        const auto engine=base_path()/"engine",exe=engine/"firefox.exe",app=engine/"app/application.ini";
        app_icon=static_cast<HICON>(LoadImageW(nullptr,(base_path()/"assets/branding/urium.ico").c_str(),IMAGE_ICON,0,0,LR_LOADFROMFILE|LR_DEFAULTSIZE));
        if(!std::filesystem::exists(exe)||!std::filesystem::exists(app))throw std::runtime_error("Le runtime Gecko embarqué est absent. Relancez tools/prepare_gecko.py puis la compilation.");
        std::filesystem::create_directories(profile/"gecko");std::filesystem::create_directories(profile/"gecko-appdata");std::filesystem::create_directories(profile/"gecko-localdata");
        {std::ofstream prefs(profile/"gecko/user.js",std::ios::trunc);prefs<<"user_pref(\"layers.acceleration.disabled\", "<<(hardware?"false":"true")<<");\nuser_pref(\"gfx.webrender.software\", "<<(hardware?"false":"true")<<");\nuser_pref(\"media.hardware-video-decoding.enabled\", "<<(hardware?"true":"false")<<");\n";if(!prefs)throw std::runtime_error("Impossible d'enregistrer le réglage GPU.");}
        std::error_code ignored;std::filesystem::remove(profile/"gecko-appdata/startup-error.txt",ignored);
        bridge=std::make_unique<LocalBridge>([this](Json message){post(std::move(message));});window_id=bridge_random(16);
        std::map<std::wstring,std::wstring> environment;
        auto* block=GetEnvironmentStringsW();for(auto* value=block;value&&*value;value+=wcslen(value)+1){std::wstring line=value;auto separator=line.find(L'=',1);if(separator!=std::wstring::npos){auto key=line.substr(0,separator);for(auto& c:key)if(c>=L'a'&&c<=L'z')c-=L'a'-L'A';environment[key]=line.substr(separator+1);}}FreeEnvironmentStringsW(block);
        environment[L"MOZ_APP_DATA"]=(profile/"gecko-appdata").wstring();environment[L"MOZ_LOCAL_APP_DATA"]=(profile/"gecko-localdata").wstring();
        environment[L"MOZ_NO_REMOTE"]=L"1";environment[L"CYURIUM_GECKO_TOKEN"]=wide(bridge->secret());environment[L"CYURIUM_GECKO_PORT"]=std::to_wstring(bridge->port());
        environment[L"CYURIUM_GECKO_WINDOW_ID"]=wide(window_id);
        environment[L"CYURIUM_GECKO_TEST"]=testing?L"1":L"0";
        environment.erase(L"XUL_APP_FILE");environment.erase(L"XRE_PROFILE_PATH");environment.erase(L"XRE_PROFILE_LOCAL_PATH");
        for(const auto* key:{L"MOZ_DISABLE_CONTENT_SANDBOX",L"MOZ_DISABLE_GPU_SANDBOX",L"MOZ_DISABLE_RDD_SANDBOX",L"MOZ_DISABLE_GMP_SANDBOX",L"MOZ_DISABLE_SOCKET_PROCESS_SANDBOX",L"MOZ_FORCE_DISABLE_E10S"})environment.erase(key);
        std::vector<wchar_t> env;for(const auto& [key,value]:environment){const auto entry=key+L"="+value;env.insert(env.end(),entry.begin(),entry.end());env.push_back(0);}env.push_back(0);
        std::wstring command=quote(exe.wstring())+L" -app "+quote(app.wstring())+L" -wait-for-browser -no-remote -profile "+quote((profile/"gecko").wstring())+L" -chrome chrome://cyurium/content/manager.xhtml";
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};log=CreateFileW((profile/"gecko-runtime.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=log;startup.hStdError=log;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION child{};
        job=CreateJobObjectW(nullptr,nullptr);if(!job)throw std::runtime_error("Impossible de créer le groupe de processus Gecko.");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits));
        if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_UNICODE_ENVIRONMENT|CREATE_SUSPENDED,env.data(),engine.c_str(),&startup,&child))throw std::runtime_error("Impossible de démarrer le runtime Gecko ("+std::to_string(GetLastError())+").");
        process=child.hProcess;process_id=child.dwProcessId;
        if(!AssignProcessToJobObject(job,process)){TerminateProcess(process,1);CloseHandle(child.hThread);throw std::runtime_error("Impossible de suivre les processus Gecko.");}
        ResumeThread(child.hThread);CloseHandle(child.hThread);
        bridge->start(process_id);
    }
#else
    void send(Json){}
    void start(){started=true;throw std::runtime_error("L'intégration native Gecko de cette version est disponible sous Windows. Le port macOS/Linux reste à réaliser.");}
#endif
    void fail(std::string reason){
        if(!failed){failed_at=std::chrono::steady_clock::now();
#ifdef _WIN32
            if(job)TerminateJobObject(job,1);
#endif
        }
        failed=true;error=std::move(reason);
    }
    ~Impl(){
#ifdef _WIN32
        if(started&&!failed){send({{"type","shutdown"}});if(process)WaitForSingleObject(process,2000);}
        bridge.reset();
        if(job)CloseHandle(job);if(process)CloseHandle(process);if(log&&log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(app_icon)DestroyIcon(app_icon);
#endif
    }
};
WebEngine::WebEngine(std::filesystem::path profile,bool testing,bool hardware):impl_(std::make_unique<Impl>()){impl_->profile=std::filesystem::absolute(std::move(profile));impl_->testing=testing;impl_->hardware=hardware;}
WebEngine::~WebEngine()=default;
bool WebEngine::pump(BrowserWorkspace& workspace){
    auto& p=*impl_;bool changed=false;
#ifdef _WIN32
    std::vector<Json> messages;{std::lock_guard guard(p.mutex);messages.swap(p.incoming);}
    for(const auto& message:messages){changed=true;try{
        const auto type=message.value("type","");const TabId id=message.value("tab",TabId(0));
        if(type=="hello"){p.connected=true;p.platform_version=message.value("version","");if(!message.value("remote",false)||!message.value("fission",false))p.fail("Gecko n’a pas activé l’isolation des processus et des sites.");}
        else if(type=="state"){
            if(auto* model=workspace.find(workspace.owner(id))){WebStatus status;status.connected=true;status.url=message.value("url","");status.title=message.value("title","");status.error=message.value("error","");status.loading=message.value("loading",false);status.can_back=message.value("canBack",false);status.can_forward=message.value("canForward",false);
                status.security=message.value("security","unknown");status.security_origin=message.value("securityOrigin","");status.security_issuer=message.value("securityIssuer","");
                model->web_state(id,std::move(status),action_value(message.value("operation","load")),message.value("request",0ull)!=0);}
        }else if(type=="tab-ready"){
            p.native[id].title=wide(message.value("title",""));
        }else if(type=="service-state"){
            auto category=message.value("category","");workspace.profile->services.state[category]=message.at("data");
            if(category=="notice"||category=="error")workspace.profile->services.notice=message.at("data").get<std::string>();
        }else if(type=="session-state"){
            if(auto* model=workspace.find(workspace.owner(id)))for(auto& tab:model->tabs)if(tab.id==id&&message.at("data").is_object()&&message.at("data").dump().size()<=131072)tab.session_state=message.at("data");
        }else if(type=="update-ready"){
            WebEvent event;event.type=type;event.value=message.value("stage","");p.events.push_back(std::move(event));
        }else if(type=="tab-crashed"){
            p.known.erase(id);p.native.erase(id);
            if(auto* model=workspace.find(workspace.owner(id)))for(auto& tab:model->tabs)if(tab.id==id){tab.web={};tab.web.loading=true;tab.web_commands.clear();tab.request(WebAction::Load,tab.current().url);}
            workspace.profile->services.notice="Un processus de page a planté. L’onglet a été rechargé.";
        }else if(type=="error")workspace.profile->services.notice="Gecko : "+message.value("message","erreur inconnue");
        else if(type=="disconnected")p.fail("Le moteur web s'est arrêté. Récupération en cours…");
        else if(type=="shortcut"||type=="new-tab"||type=="inspection"||type=="extension-tab-close"||type=="automation-result"){
            WebEvent event;event.type=type;event.tab=type=="new-tab"?message.value("opener",TabId(0)):id;
            event.key=message.value("key","");event.url=message.value("url","");event.control=message.value("control",false);event.shift=message.value("shift",false);event.alt=message.value("alt",false);event.request=message.value("request",0ull);event.message=message.value("error","");
            if(message.contains("data")){const auto& d=message["data"];event.data=d;event.title=d.value("title","");event.url=d.value("url","");event.value=d.value("value","");event.instance=d.value("instance","");event.process=d.value("process",0ull);}
            p.events.push_back(std::move(event));
        }
    }catch(const std::exception& e){p.fail(std::string("Réponse Gecko invalide : ")+e.what());}}
#endif
    if(p.started&&!p.connected&&!p.failed&&std::chrono::steady_clock::now()-p.launched>std::chrono::seconds(25)){p.fail("Gecko ne répond pas. Consultez gecko-runtime.log dans le profil.");changed=true;}
    if(p.started&&!p.connected&&!p.failed){std::ifstream startup(p.profile/"gecko-appdata/startup-error.txt");if(startup){std::string reason;std::getline(startup,reason);if(!reason.empty()){p.fail("Démarrage Gecko : "+reason);changed=true;}}}
    if(p.failed)for(auto id:workspace.window_ids())for(auto& tab:workspace.find(id)->tabs)if(tab.current().page==Page::Pending){if(tab.web.error!=p.error)changed=true;tab.web.error=p.error;tab.web.loading=false;}
    return changed;
}
void WebEngine::sync(BrowserWorkspace& workspace,std::span<const WebPlacement> placements){
    if(impl_->failed&&impl_->failed_at-impl_->launched>std::chrono::seconds(60))restarts_=0;
    if(impl_->failed&&impl_->connected&&restarts_<3&&std::chrono::steady_clock::now()-impl_->failed_at>std::chrono::seconds(1)){
        ++restarts_;restart(workspace,workspace.profile->services.hardware_acceleration);
        workspace.profile->services.notice="Le moteur web a redémarré après un plantage. Les pages ont été rouvertes.";
    }
    auto& p=*impl_;std::set<TabId> alive;
    auto& services=workspace.profile->services;
    if(!services.prompt_window||!workspace.find(services.prompt_window))services.prompt_window=0;
    if(std::any_of(services.commands.begin(),services.commands.end(),[](const auto& m){return m.value("type","")=="engine-restart";})){
        std::erase_if(services.commands,[](const auto& m){return m.value("type","")=="engine-restart";});
        restarts_=0;restart(workspace,services.hardware_acceleration);sync(workspace,placements);return;
    }
    bool needs=false;for(auto id:workspace.window_ids())for(auto& tab:workspace.find(id)->tabs){alive.insert(tab.id);if(tab.current().page==Page::Pending)needs=true;}
    if((needs||!services.commands.empty())&&!p.started)try{p.start();}catch(const std::exception& e){p.fail(e.what());}
    if(p.connected&&!p.failed){
        if(p.form_policy!=services.restore_forms){
            p.form_policy=services.restore_forms;p.send({{"type","session-policy"},{"restoreForms",services.restore_forms}});
            if(!services.restore_forms)for(auto id:workspace.window_ids())for(auto& tab:workspace.find(id)->tabs)if(tab.session_state.is_object())tab.session_state.erase("fields");
        }
        Json layout={{"type","workspace-layout"},{"windows",Json::array()}};
        for(auto id:workspace.window_ids()){const auto& model=*workspace.find(id);Json ids=Json::array();for(auto& t:model.tabs)ids.push_back(t.id);layout["windows"].push_back({{"id",id},{"selected",model.selected},{"tabs",ids}});}
        if(layout!=p.layout){p.layout=layout;p.send(layout);}
        for(auto& command:services.commands)p.send(std::move(command));services.commands.clear();
        for(auto id:p.known)if(!alive.contains(id)){p.send({{"type","close"},{"tab",id}});
#ifdef _WIN32
            p.native.erase(id);
#endif
        }
        std::erase_if(p.known,[&](auto id){return !alive.contains(id);});
        for(auto id:workspace.window_ids())for(auto& tab:workspace.find(id)->tabs){
            if(tab.current().page!=Page::Pending)continue;
            if(p.known.insert(tab.id).second){p.send({{"type","create"},{"tab",tab.id}});if(tab.session_state.is_object())p.send({{"type","restore"},{"tab",tab.id},{"data",tab.session_state}});}
            for(const auto& command:tab.web_commands)p.send({{"type","navigate"},{"tab",tab.id},{"sequence",command.sequence},{"action",action_name(command.action)},{"url",command.url}});
            tab.web_commands.clear();
        }
    }
#ifdef _WIN32
    if(p.connected&&!p.manager_hidden)if(auto manager=find_window(wide("CyUrium Gecko Manager "+p.window_id),p.process_id)){
        if(p.app_icon){SendMessageW(manager,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(p.app_icon));SendMessageW(manager,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(p.app_icon));}
        ShowWindow(manager,SW_HIDE);auto style=GetWindowLongPtrW(manager,GWL_EXSTYLE);
        SetWindowLongPtrW(manager,GWL_EXSTYLE,(style&~WS_EX_APPWINDOW)|WS_EX_TOOLWINDOW);p.manager_hidden=true;
    }
    for(auto& [id,native]:p.native){
        const bool active=!p.failed&&std::any_of(placements.begin(),placements.end(),[&](const auto& v){return v.tab==id&&v.visible;});
        if(!native.active||*native.active!=active){native.active=active;p.send({{"type","active"},{"tab",id},{"value",active}});}
        if(!native.window&&!native.title.empty()){
            native.window=find_window(native.title,p.process_id);
            if(native.window&&p.app_icon){SendMessageW(native.window,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(p.app_icon));SendMessageW(native.window,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(p.app_icon));}
        }
        if(!native.window||!IsWindow(native.window))continue;
        auto it=std::find_if(placements.begin(),placements.end(),[&](const auto& v){return v.tab==id&&v.window&&v.visible;});
        if(it==placements.end()||p.failed){
            if(native.visible)ShowWindow(native.window,SW_HIDE);native.visible=false;
            // Park a surviving tab before ImGui destroys its former OS window.
            // The destination viewport may only be created later in this frame.
            if(native.parent&&std::none_of(placements.begin(),placements.end(),[&](const auto& v){return v.window&&hwnd(v.window)==native.parent;})){
                SetParent(native.window,nullptr);auto style=GetWindowLongPtrW(native.window,GWL_STYLE);
                SetWindowLongPtrW(native.window,GWL_STYLE,(style&~WS_CHILD)|WS_POPUP);native.parent=nullptr;
            }
            continue;
        }
        const auto parent=hwnd(it->window);
        if(native.parent!=parent){
            SetLastError(0);SetParent(native.window,parent);
            if(GetLastError()){p.fail("Impossible de rattacher la surface Gecko à la fenêtre.");continue;}
            auto style=GetWindowLongPtrW(native.window,GWL_STYLE);style&=~(WS_POPUP|WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);style|=WS_CHILD|WS_CLIPSIBLINGS;
            SetWindowLongPtrW(native.window,GWL_STYLE,style);SetWindowLongPtrW(parent,GWL_STYLE,GetWindowLongPtrW(parent,GWL_STYLE)|WS_CLIPCHILDREN);
            native.parent=parent;
            native.visible=false;
        }
        POINT origin{};ClientToScreen(parent,&origin);Rect bounds{it->content.x-origin.x,it->content.y-origin.y,it->content.w,it->content.h};
        if(!native.visible||bounds.x!=native.bounds.x||bounds.y!=native.bounds.y||bounds.w!=native.bounds.w||bounds.h!=native.bounds.h){
            SetWindowPos(native.window,HWND_TOP,int(bounds.x),int(bounds.y),std::max(1,int(bounds.w)),std::max(1,int(bounds.h)),SWP_NOACTIVATE|SWP_FRAMECHANGED|SWP_SHOWWINDOW);native.bounds=bounds;native.visible=true;
        }
        if(it->focus_ui&&GetFocus()!=parent){SetFocus(parent);SDL_RaiseWindow(it->window);}
    }
#endif
}
std::vector<WebEvent> WebEngine::take_events(){auto result=std::move(impl_->events);impl_->events.clear();return result;}
void WebEngine::inspect(TabId id,std::uint64_t request){impl_->send({{"type","inspect"},{"tab",id},{"request",request}});}
void WebEngine::automate(TabId id,std::uint64_t request,std::string operation,Json parameters){impl_->send({{"type","automation"},{"tab",id},{"request",request},{"operation",std::move(operation)},{"parameters",std::move(parameters)}});}
bool WebEngine::ready()const{return impl_->connected&&!impl_->failed;}
std::size_t WebEngine::native_views()const{
#ifdef _WIN32
return impl_->native.size();
#else
return 0;
#endif
}
std::string WebEngine::version()const{return impl_->platform_version;}
void WebEngine::restart(BrowserWorkspace& workspace,bool hardware){
    auto profile=impl_->profile;auto testing=impl_->testing;impl_.reset();impl_=std::make_unique<Impl>();impl_->profile=profile;impl_->testing=testing;impl_->hardware=hardware;
    workspace.profile->services.state.erase("prompts");workspace.profile->services.state["ready"]=false;
    for(auto id:workspace.window_ids())for(auto& tab:workspace.find(id)->tabs){tab.web={};tab.web_commands.clear();if(tab.current().page==Page::Pending){tab.web.loading=true;tab.request(WebAction::Load,tab.current().url);}}
    // Start even if only a settings page is visible.
    workspace.profile->services.request("services-refresh");
}
void WebEngine::test_crash(){
#ifdef _WIN32
    if(impl_->testing&&impl_->process)TerminateProcess(impl_->process,91);
#endif
}
bool WebEngine::embedded(TabId id,SDL_Window* window)const{
#ifdef _WIN32
    const auto it=impl_->native.find(id);return window&&it!=impl_->native.end()&&IsWindow(it->second.window)&&GetParent(it->second.window)==hwnd(window)&&IsWindowVisible(it->second.window);
#else
    return false;
#endif
}
}
