// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_workspace.hpp"
#include "tab_drag.hpp"
#include "browser_view.hpp"
#include "browser_theme.hpp"
#include "web_engine.hpp"
#include "browser_sdk.hpp"
#include "session_store.hpp"
#include "updater.hpp"
#include "json.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

using namespace cyurium;
namespace {
std::filesystem::path utf8_path(std::string_view text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),text.size()));
}
std::string utf8(const std::filesystem::path& path) {
    auto value=path.u8string();return {value.begin(),value.end()};
}
struct Options {
    std::filesystem::path capture, metrics, profile, window_test,web_test,update_health,assistant_test;
    std::string scenario = "home", driver, interface_style = "classic",url;
    int width = 1360, height = 880;
    double seconds = 0;
    bool background_test=false,mcp=false;
};
Options options(int argc,char** argv) {
    Options result;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        auto next=[&](){if(i+1>=argc)throw std::runtime_error("Missing value for "+arg);return std::string(argv[++i]);};
        if(arg=="--capture")result.capture=utf8_path(next());
        else if(arg=="--background-test")result.background_test=true;
        else if(arg=="--mcp")result.mcp=true;
        else if(arg=="--assistant-test")result.assistant_test=utf8_path(next());
        else if(arg=="--update-health")result.update_health=utf8_path(next());
        else if(arg=="--window-test")result.window_test=utf8_path(next());
        else if(arg=="--web-test")result.web_test=utf8_path(next());
        else if(arg=="--url")result.url=next();
        else if(arg=="--metrics")result.metrics=utf8_path(next());
        else if(arg=="--profile")result.profile=utf8_path(next());
        else if(arg=="--scenario")result.scenario=next();
        else if(arg=="--driver")result.driver=next();
        else if(arg=="--interface")result.interface_style=next();
        else if(arg=="--width")result.width=std::stoi(next());
        else if(arg=="--height")result.height=std::stoi(next());
        else if(arg=="--idle-seconds")result.seconds=std::stod(next());
        else throw std::runtime_error("Unknown option: "+arg);
    }
    if(result.width<720||result.width>7680||result.height<520||result.height>4320||result.seconds<0||result.seconds>3600)
        throw std::runtime_error("Invalid window size or duration");
    return result;
}
void require(bool ok,const char* operation) {
    if(!ok)throw std::runtime_error(std::string(operation)+": "+SDL_GetError());
}
SDL_GPUTexture* load_logo(SDL_GPUDevice* device,SDL_Surface*& icon,std::vector<unsigned char>& pixels) {
    const auto path=utf8_path(SDL_GetBasePath())/"assets/branding/urium-logo.rgba";
    std::ifstream input(path,std::ios::binary);pixels.assign(std::istreambuf_iterator<char>(input),{});
    if(pixels.size()!=256*256*4)throw std::runtime_error("Missing or invalid Urium logo asset");
    icon=SDL_CreateSurfaceFrom(256,256,SDL_PIXELFORMAT_RGBA32,pixels.data(),256*4);require(icon!=nullptr,"Create Urium icon");
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;info.width=256;info.height=256;info.layer_count_or_depth=1;info.num_levels=1;
    auto* texture=SDL_CreateGPUTexture(device,&info);require(texture!=nullptr,"Create Urium logo texture");
    SDL_GPUTransferBufferCreateInfo transfer_info{};transfer_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;transfer_info.size=Uint32(pixels.size());
    auto* transfer=SDL_CreateGPUTransferBuffer(device,&transfer_info);require(transfer!=nullptr,"Create Urium logo upload");
    auto* mapped=SDL_MapGPUTransferBuffer(device,transfer,false);require(mapped!=nullptr,"Map Urium logo upload");
    SDL_memcpy(mapped,pixels.data(),pixels.size());SDL_UnmapGPUTransferBuffer(device,transfer);
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command!=nullptr,"Acquire Urium logo upload buffer");
    auto* copy=SDL_BeginGPUCopyPass(command);SDL_GPUTextureTransferInfo source{};source.transfer_buffer=transfer;
    SDL_GPUTextureRegion destination{};destination.texture=texture;destination.w=256;destination.h=256;destination.d=1;
    SDL_UploadToGPUTexture(copy,&source,&destination,false);SDL_EndGPUCopyPass(copy);require(SDL_SubmitGPUCommandBuffer(command),"Upload Urium logo");
    SDL_ReleaseGPUTransferBuffer(device,transfer);return texture;
}
double cpu_seconds() {
#ifdef _WIN32
    FILETIME creation,exit,kernel,user;
    if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user))return 0;
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
    return double(k.QuadPart+u.QuadPart)*1e-7;
#else
    rusage usage{};getrusage(RUSAGE_SELF,&usage);
    return usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6;
#endif
}
double resident_mb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if(GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters)))return double(counters.WorkingSetSize)/1048576.;
    return 0;
#else
    rusage usage{};getrusage(RUSAGE_SELF,&usage);
#ifdef __APPLE__
    return double(usage.ru_maxrss)/1048576.;
#else
    return double(usage.ru_maxrss)/1024.;
#endif
#endif
}
struct HitRegions { float begin=350,end=1200,scale=1; };
SDL_HitTestResult SDLCALL hit_test(SDL_Window* window,const SDL_Point* point,void* data) {
    const auto& region=*static_cast<HitRegions*>(data);
    int width,height;SDL_GetWindowSize(window,&width,&height);
    const int edge=6;
    if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_MAXIMIZED)) {
        if(point->y<edge){if(point->x<edge)return SDL_HITTEST_RESIZE_TOPLEFT;if(point->x>=width-edge)return SDL_HITTEST_RESIZE_TOPRIGHT;return SDL_HITTEST_RESIZE_TOP;}
        if(point->y>=height-edge){if(point->x<edge)return SDL_HITTEST_RESIZE_BOTTOMLEFT;if(point->x>=width-edge)return SDL_HITTEST_RESIZE_BOTTOMRIGHT;return SDL_HITTEST_RESIZE_BOTTOM;}
        if(point->x<edge)return SDL_HITTEST_RESIZE_LEFT;
        if(point->x>=width-edge)return SDL_HITTEST_RESIZE_RIGHT;
    }
    if(point->y<43*region.scale && point->x>region.begin && point->x<region.end)return SDL_HITTEST_DRAGGABLE;
    return SDL_HITTEST_NORMAL;
}
void capture(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command,ImDrawData* draw,
             SDL_GPUTextureFormat format,int width,int height,const std::filesystem::path& path) {
    SDL_GPUTextureCreateInfo info{};
    info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width=width;info.height=height;info.layer_count_or_depth=1;info.num_levels=1;info.sample_count=SDL_GPU_SAMPLECOUNT_1;
    auto* target=SDL_CreateGPUTexture(device,&info);require(target!=nullptr,"Create capture texture");
    SDL_GPUColorTargetInfo color{};color.texture=target;color.load_op=SDL_GPU_LOADOP_CLEAR;color.store_op=SDL_GPU_STOREOP_STORE;
    color.clear_color={1,1,1,1};
    auto* pass=SDL_BeginGPURenderPass(command,&color,1,nullptr);require(pass!=nullptr,"Begin capture pass");
    ImGui_ImplSDLGPU3_RenderDrawData(draw,command,pass);SDL_EndGPURenderPass(pass);
    const Uint32 pitch=(Uint32(width)*4+255)&~Uint32(255);
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer_info.size=pitch*height;
    auto* transfer=SDL_CreateGPUTransferBuffer(device,&transfer_info);require(transfer!=nullptr,"Create capture readback");
    auto* copy=SDL_BeginGPUCopyPass(command);require(copy!=nullptr,"Begin capture copy");
    SDL_GPUTextureRegion source{};source.texture=target;source.w=width;source.h=height;source.d=1;
    SDL_GPUTextureTransferInfo destination{};destination.transfer_buffer=transfer;destination.pixels_per_row=pitch/4;destination.rows_per_layer=height;
    SDL_DownloadFromGPUTexture(copy,&source,&destination);SDL_EndGPUCopyPass(copy);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence!=nullptr,"Submit capture");
    require(SDL_WaitForGPUFences(device,true,&fence,1),"Wait for capture");
    void* pixels=SDL_MapGPUTransferBuffer(device,transfer,false);require(pixels!=nullptr,"Map capture");
    bool bgra=format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM||format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    auto* surface=SDL_CreateSurfaceFrom(width,height,bgra?SDL_PIXELFORMAT_BGRA32:SDL_PIXELFORMAT_RGBA32,pixels,pitch);
    require(surface!=nullptr,"Create capture surface");
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    require(SDL_SaveBMP(surface,utf8(path).c_str()),"Save capture");
    SDL_DestroySurface(surface);SDL_UnmapGPUTransferBuffer(device,transfer);
    SDL_ReleaseGPUFence(device,fence);SDL_ReleaseGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTexture(device,target);
}
}

struct NativeView {
    explicit NativeView(std::string_view style):view(make_browser_view(style)){}
    std::unique_ptr<BrowserView> view;
    UiActions last;
    Rect placement;
    bool primary=false,place=true;
    SDL_WindowID native=0;
    HitRegions hits;
    ~NativeView(){if(native)if(auto* window=SDL_GetWindowFromID(native))SDL_SetWindowHitTest(window,nullptr,nullptr);}
};
SDL_Window* native_window(std::uint32_t viewport) {
    auto* vp=ImGui::FindViewportByID(viewport);
    return vp?SDL_GetWindowFromID(static_cast<SDL_WindowID>(reinterpret_cast<intptr_t>(vp->PlatformHandle))):nullptr;
}

int main(int argc,char** argv) {
    if(auto result=run_update_helper(argc,argv);result>=0)return result;
    SDL_Window* window=nullptr;SDL_GPUDevice* device=nullptr;SDL_GPUTexture* logo=nullptr;SDL_Surface* icon=nullptr;
    std::vector<unsigned char> logo_pixels;std::unique_ptr<WebEngine> web_engine;bool context=false,platform=false,renderer=false;
    try {
        const Options config=options(argc,argv);
        if(config.mcp){
            auto* pref=SDL_GetPrefPath("CyUrium","CyUrium");
            auto mcp_profile=std::filesystem::absolute(config.profile.empty()?(pref?utf8_path(pref):utf8_path(SDL_GetBasePath())/"profile"):config.profile);SDL_free(pref);
            return run_mcp_bridge(utf8_path(SDL_GetBasePath()),mcp_profile);
        }
        if(!config.web_test.empty()||config.background_test)SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN,"0");
#if defined(__linux__)
        // SDL's ImGui multi-viewport backend needs global pointer coordinates.
        // XWayland supplies them; native Wayland does not currently do so.
        if(!SDL_GetHint(SDL_HINT_VIDEO_DRIVER)&&std::getenv("DISPLAY"))SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"x11");
#endif
        require(SDL_Init(SDL_INIT_VIDEO),"SDL_Init");
        float dpi=SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());if(dpi<=0)dpi=1;
        window=SDL_CreateWindow("CyUrium",int(config.width*dpi),int(config.height*dpi),
            SDL_WINDOW_RESIZABLE|SDL_WINDOW_BORDERLESS|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_HIDDEN);
        require(window!=nullptr,"Create window");
        SDL_SetWindowMinimumSize(window,720,520);
        if(!config.web_test.empty()||config.background_test)SDL_SetWindowPosition(window,-10000,-10000);
        else SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
        const char* preferred=nullptr;
#ifdef _WIN32
        preferred="direct3d12";
#elif defined(__APPLE__)
        preferred="metal";
#else
        preferred="vulkan";
#endif
        if(!config.driver.empty())preferred=config.driver.c_str();
        constexpr auto formats=SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_METALLIB;
        device=SDL_CreateGPUDevice(formats,false,preferred);
        if(!device&&config.driver.empty())device=SDL_CreateGPUDevice(formats,false,nullptr);
        require(device!=nullptr,"Create GPU device");require(SDL_ClaimWindowForGPUDevice(device,window),"Claim GPU window");
        require(SDL_SetGPUSwapchainParameters(device,window,SDL_GPU_SWAPCHAINCOMPOSITION_SDR,SDL_GPU_PRESENTMODE_VSYNC),"Configure swapchain");
        SDL_SetGPUAllowedFramesInFlight(device,2);const auto format=SDL_GetGPUSwapchainTextureFormat(device,window);
        IMGUI_CHECKVERSION();ImGui::CreateContext();context=true;
        auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoAutoMerge=true;io.ConfigViewportsNoTaskBarIcon=false;
        io.IniFilename=nullptr;io.LogFilename=nullptr;apply_browser_theme(false,dpi);
        const auto asset_path=utf8_path(SDL_GetBasePath())/"assets"/"Roboto-Medium.ttf";
        if(std::filesystem::exists(asset_path))io.Fonts->AddFontFromFileTTF(utf8(asset_path).c_str(),14);
        else io.Fonts->AddFontDefault();
        require(ImGui_ImplSDL3_InitForSDLGPU(window),"Init SDL ImGui backend");platform=true;
        if(!(io.BackendFlags&ImGuiBackendFlags_PlatformHasViewports))
            throw std::runtime_error("This display backend cannot create independent browser windows. On Linux, use X11 or XWayland.");
        ImGui_ImplSDLGPU3_InitInfo init{};init.Device=device;init.ColorTargetFormat=format;
        init.MSAASamples=SDL_GPU_SAMPLECOUNT_1;init.PresentMode=SDL_GPU_PRESENTMODE_VSYNC;
        require(ImGui_ImplSDLGPU3_Init(&init),"Init GPU ImGui backend");renderer=true;
        logo=load_logo(device,icon,logo_pixels);set_browser_logo(ImTextureRef(reinterpret_cast<ImTextureID>(logo)));SDL_SetWindowIcon(window,icon);
        BrowserWorkspace workspace;auto primary=workspace.create_window();
        BrowserModel profile_model(workspace.profile,false);
        auto* preference_path=SDL_GetPrefPath("CyUrium","CyUrium");
        const auto profile=std::filesystem::absolute(config.profile.empty()?(preference_path?utf8_path(preference_path):utf8_path(SDL_GetBasePath())/"profile"):config.profile);
        SDL_free(preference_path);
        const auto state_file=profile/"settings.txt";
        const bool qa=!config.window_test.empty();
        const bool web_qa=!config.web_test.empty();
        if(web_qa||config.background_test)io.ConfigFlags|=ImGuiConfigFlags_NoMouse;
        if(config.capture.empty()&&!qa&&!profile_model.load(state_file)) {auto backup=state_file;backup+=".bak";profile_model.load(backup);}
        SessionStore sessions(profile);const bool session_enabled=config.capture.empty()&&!qa&&!web_qa;
        if(session_enabled&&workspace.profile->services.restore_session&&sessions.restore(workspace)){
            primary=workspace.window_ids().front();workspace.profile->services.recovered_session=sessions.recovered();
            if(sessions.recovered())workspace.profile->services.notice="Session récupérée après un arrêt inattendu.";
        }
        if(session_enabled)sessions.checkpoint(workspace);
        std::map<WindowId,std::unique_ptr<NativeView>> views;
        std::vector<WindowId> front_to_back;
        WindowId focused=primary;
        auto create_view=[&](WindowId id,Rect bounds,bool first=false) {
            auto view=std::make_unique<NativeView>(config.interface_style);view->primary=first;view->placement=bounds;
            if(first)view->native=SDL_GetWindowID(window);
            views.emplace(id,std::move(view));front_to_back.insert(front_to_back.begin(),id);
        };
        int wx{},wy{};SDL_GetWindowPosition(window,&wx,&wy);
        if(auto it=workspace.placements.find(primary);it!=workspace.placements.end()){
            const auto& b=it->second;int displays_count=0;auto* displays=SDL_GetDisplays(&displays_count);bool on_screen=false;
            for(int i=0;i<displays_count;++i){SDL_Rect area{};if(SDL_GetDisplayUsableBounds(displays[i],&area)&&b[0]+100>area.x&&b[1]+40>area.y&&b[0]<area.x+area.w&&b[1]<area.y+area.h)on_screen=true;}SDL_free(displays);
            if(on_screen){SDL_SetWindowPosition(window,int(b[0]),int(b[1]));SDL_SetWindowSize(window,int(b[2]),int(b[3]));wx=int(b[0]);wy=int(b[1]);}
        }
        create_view(primary,{float(wx),float(wy),config.width*dpi,config.height*dpi},true);
        for(auto id:workspace.window_ids())if(id!=primary){auto it=workspace.placements.find(id);if(it!=workspace.placements.end()){auto b=it->second;create_view(id,{b[0],b[1],b[2],b[3]});}else create_view(id,{float(wx)+48*float(id-1),float(wy)+48*float(id-1),config.width*dpi,config.height*dpi});}
        auto raise=[&](WindowId id) {
            focused=id;std::erase(front_to_back,id);front_to_back.insert(front_to_back.begin(),id);
            if(auto it=views.find(id);it!=views.end())if(auto* native=SDL_GetWindowFromID(it->second->native))SDL_RaiseWindow(native);
        };
        auto new_window=[&](Rect bounds) {
            auto id=workspace.create_window();create_view(id,bounds);focused=id;return id;
        };
        auto detach=[&](WindowId from,TabId tab,Point at) {
            auto it=views.find(from);if(it==views.end())return WindowId(0);
            auto bounds=it->second->last.bounds;bounds.x=at.x;bounds.y=at.y;
            auto id=workspace.detach_tab(from,tab);
            if(id){create_view(id,bounds);focused=id;}return id;
        };
        auto sync_views=[&]() {
            for(auto it=views.begin();it!=views.end();)if(!workspace.find(it->first)) {
                if(it->second->primary)SDL_HideWindow(window);
                std::erase(front_to_back,it->first);it=views.erase(it);
            } else ++it;
            if(!workspace.find(focused)&&!front_to_back.empty())focused=front_to_back.front();
        };
        auto& initial=*workspace.find(primary);
        if(!config.url.empty())initial.navigate(config.url);
        if(config.scenario=="dark")workspace.profile->theme=Theme::Dark;
        else if(config.scenario=="settings")initial.open(Page::Settings);
        else if(config.scenario=="dark-settings"){workspace.profile->theme=Theme::Dark;initial.open(Page::Settings);}
        else if(config.scenario=="menu")views.at(primary)->view->show_menu();
        else if(config.scenario=="tabs"){for(const char* url:{"github.com","developer.mozilla.org","mozilla.org"}){initial.navigate(url);initial.new_tab();}}
        else if(config.scenario=="pending")initial.navigate("https://www.mozilla.org");
        else if(config.scenario=="bookmarks")initial.open(Page::Bookmarks);
        else if(config.scenario=="assistant")workspace.profile->services.assistant_visible=true;
        else if(config.scenario!="home")throw std::runtime_error("Unknown capture scenario");
        if(!config.assistant_test.empty()){
            if(!config.url.starts_with("http://127.0.0.1:"))throw std::runtime_error("Assistant QA requires an explicit local fixture");
            workspace.profile->services.assistant_visible=true;
        }
        TabId test_tab=0;WindowId test_second=0;int test_tick=0;bool test_complete=false;
        if(qa){initial.new_tab();initial.new_tab();test_tab=initial.selected;initial.navigate("example.org");initial.navigate("mozilla.org");initial.back();}
        SDL_ShowWindow(window);
        if(!qa&&config.capture.empty())web_engine=std::make_unique<WebEngine>(profile,web_qa,workspace.profile->services.hardware_acceleration);
        BrowserSdk sdk(utf8_path(SDL_GetBasePath()),profile);
        if(!config.update_health.empty())workspace.profile->services.request("graphics-refresh");
        TabDragController drag;
        const auto web_tab=initial.selected;int web_phase=0,web_frame=0;
        const Page web_internal_pages[]={Page::Settings,Page::Bookmarks,Page::History,Page::Downloads,Page::Extensions};
        std::size_t web_internal_index=0;TabId web_internal_tab=0;
        std::string web_instance,web_value;std::uint64_t content_process=0;
        bool web_complete=false,web_forward_checked=false;std::string final_web_url;
        int assistant_phase=0;bool assistant_streamed=false,assistant_complete=false;
        auto web_check=[](bool condition,const char* message){if(!condition)throw std::runtime_error(std::string("Web test: ")+message);};
        bool done=false,dirty=true,save_pending=false;int frames=0;double cpu_sum=0,cpu_max=0;
        const auto start=std::chrono::steady_clock::now();const double process_start=cpu_seconds();
        auto awake_until=start+std::chrono::milliseconds(650),changed_at=start;
        auto checkpoint_at=start;
        auto handle=[&](SDL_Event& event) {
            const bool closing_key=event.type==SDL_EVENT_KEY_DOWN&&(event.key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))&&(event.key.key==SDLK_W||event.key.key==SDLK_Q);
            if(session_enabled&&(event.type==SDL_EVENT_QUIT||event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED||closing_key))sessions.checkpoint(workspace);
            ImGui_ImplSDL3_ProcessEvent(&event);dirty=true;
            awake_until=std::chrono::steady_clock::now()+std::chrono::milliseconds(650);
            if(event.type==SDL_EVENT_QUIT)done=true;
            WindowId id=focused,event_owner=0;
            SDL_WindowID event_window=0;
            if(event.type>=SDL_EVENT_WINDOW_FIRST&&event.type<=SDL_EVENT_WINDOW_LAST)event_window=event.window.windowID;
            else if(event.type==SDL_EVENT_KEY_DOWN)event_window=event.key.windowID;
            else if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN)event_window=event.button.windowID;
            for(const auto& [key,view]:views)if(view->native==event_window){id=key;event_owner=key;break;}
            if(event_owner&&(event.type==SDL_EVENT_WINDOW_FOCUS_GAINED||event.type==SDL_EVENT_MOUSE_BUTTON_DOWN))
                {focused=id;std::erase(front_to_back,id);front_to_back.insert(front_to_back.begin(),id);}
            if(event_owner&&event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED){workspace.close_window(id);sync_views();}
            auto* model=workspace.find(id);auto it=views.find(id);
            if(!model||it==views.end())return;
            auto& ui=*it->second->view;auto* native=SDL_GetWindowFromID(it->second->native);
            if(event.type==SDL_EVENT_KEY_DOWN&&!event.key.repeat) {
                const auto mods=event.key.mod;
#ifdef __APPLE__
                const bool control=(mods&SDL_KMOD_GUI)!=0;
#else
                const bool control=(mods&SDL_KMOD_CTRL)!=0;
#endif
                const bool shift=(mods&SDL_KMOD_SHIFT)!=0;
                if(control) {
                    if(event.key.key==SDLK_Q){done=true;return;}
                    if(event.key.key==SDLK_W){if(shift)workspace.close_window(id);else workspace.close_tab(id,model->selected);sync_views();return;}
                    if(event.key.key==SDLK_N){auto b=it->second->last.bounds;b.x+=48;b.y+=56;new_window(b);return;}
                    if(event.key.key==SDLK_T){model->new_tab();ui.focus_address();}
                    else if(event.key.key==SDLK_R)model->reload();
                    else if(event.key.key==SDLK_L)ui.focus_address();
                    else if(event.key.key==SDLK_TAB)model->cycle(shift?-1:1);
                    else if(event.key.key==SDLK_COMMA)model->open(Page::Settings);
                    else if(event.key.key==SDLK_H)model->open(Page::History);
                    else if(event.key.key==SDLK_J)model->open(Page::Downloads);
                    else if(event.key.key==SDLK_D)model->toggle_bookmark();
                    else if(event.key.key==SDLK_B&&shift)model->profile->bookmarks_visible=!model->profile->bookmarks_visible;
                    else if(event.key.key==SDLK_EQUALS||event.key.key==SDLK_PLUS)model->profile->ui_scale=std::min(1.5f,model->profile->ui_scale+.05f);
                    else if(event.key.key==SDLK_MINUS)model->profile->ui_scale=std::max(.85f,model->profile->ui_scale-.05f);
                    else if(event.key.key==SDLK_0)model->profile->ui_scale=1;
                    else if(event.key.key>=SDLK_1&&event.key.key<=SDLK_9){auto i=event.key.key==SDLK_9?model->tabs.size()-1:std::size_t(event.key.key-SDLK_1);if(i<model->tabs.size())model->select(model->tabs[i].id);}
                    save_pending=true;changed_at=std::chrono::steady_clock::now();
                }
                if(mods&SDL_KMOD_ALT){if(event.key.key==SDLK_LEFT)model->back();if(event.key.key==SDLK_RIGHT)model->forward();}
                if(event.key.key==SDLK_F6)ui.focus_address();
                if(event.key.key==SDLK_F5)model->reload();
                if(event.key.key==SDLK_ESCAPE)model->stop();
                if(event.key.key==SDLK_F11&&native)SDL_SetWindowFullscreen(native,!(SDL_GetWindowFlags(native)&SDL_WINDOW_FULLSCREEN));
            }
        };
        auto test_check=[](bool okay,const char* message){if(!okay)throw std::runtime_error(std::string("Native window test: ")+message);};
        // QA injects input into this application's ImGui queue. It never drives the user's desktop.
        auto inject_test=[&]() {
            const auto tick=test_tick++;
            auto point=[&](WindowId id,std::size_t index){const auto& s=views.at(id)->last.tab_strip.slots.at(index);return Point{s.body.x+25,s.body.y+17};};
            auto mouse=[&](Point p){io.AddMousePosEvent(p.x,p.y);io.AddMouseViewportEvent(views.at(workspace.owner(test_tab))->last.viewport);};
            if(tick==12){mouse(point(primary,2));io.AddMouseButtonEvent(0,true);}
            if(tick==14){mouse(point(primary,0));}
            if(tick==16)io.AddMouseButtonEvent(0,false);
            if(tick==20){test_check(initial.tabs.front().id==test_tab,"tab reorder failed");mouse(point(primary,0));io.AddMouseButtonEvent(0,true);}
            if(tick==22){auto b=views.at(primary)->last.bounds;mouse({b.x+100,b.y+210});}
            if(tick==24)io.AddMouseButtonEvent(0,false);
            if(tick==30) {
                auto owner=workspace.owner(test_tab);test_check(owner!=primary,"tab detach did not create a window");
                auto* native=SDL_GetWindowFromID(views.at(owner)->native);
                test_check(native!=nullptr,"detached OS window disappeared");
                require(SDL_SetWindowSize(native,1000,700),"Resize detached window for QA");
            }
            if(tick==36){
                test_second=workspace.owner(test_tab);test_check(test_second&&test_second!=primary,"tab detach failed");
                test_check(views.at(test_second)->native&&views.at(test_second)->native!=SDL_GetWindowID(window),"second OS window missing");
                auto* native=SDL_GetWindowFromID(views.at(test_second)->native);
                test_check(native&&!SDL_GetWindowParent(native),"detached window has a native parent");
                test_check((SDL_GetWindowFlags(native)&SDL_WINDOW_RESIZABLE)!=0,"detached window is not resizable");
                const auto bounds=views.at(test_second)->last.bounds;
                test_check(bounds.w==1000&&bounds.h==700,"native resize did not update the presentation");
                auto& tab=workspace.find(test_second)->active();test_check(tab.id==test_tab&&tab.history.size()==3&&tab.can_forward(),"detach lost history");
                mouse(point(test_second,0));io.AddMouseButtonEvent(0,true);
            }
            if(tick==38){mouse(point(primary,0));}
            if(tick==40)io.AddMouseButtonEvent(0,false);
            if(tick==48){
                test_check(workspace.owner(test_tab)==primary&&workspace.size()==1,"cross-window tab attach failed");
                test_check(initial.active().can_forward(),"attach lost forward history");
                auto b=views.at(primary)->last.bounds;b.x+=80;b.y+=100;test_second=new_window(b);
            }
            if(tick==58){
                test_check(views.at(test_second)->native!=SDL_GetWindowID(window),"new OS window missing");
                // Exercise the same native close event handler as the window-manager close button.
                SDL_Event close{};close.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;close.window.windowID=SDL_GetWindowID(window);handle(close);
            }
            if(tick==60){
                SDL_Event stale{};stale.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;stale.window.windowID=SDL_GetWindowID(window);handle(stale);
                test_check(workspace.size()==1,"a stale close event affected the surviving window");
            }
            if(tick==68){
                test_check(workspace.size()==1&&workspace.find(test_second),"closing first window killed the other window");
                test_check((SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN)!=0,"primary service window stayed visible");
                auto* native=SDL_GetWindowFromID(views.at(test_second)->native);
                test_check(native&&(SDL_GetWindowFlags(native)&SDL_WINDOW_HIDDEN)==0,"surviving OS window is hidden");
                SDL_Event close{};close.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;close.window.windowID=SDL_GetWindowID(native);handle(close);
                test_check(workspace.size()==0,"closing last window did not finish");test_complete=true;
            }
        };
        while(!done&&workspace.size()) {
            auto now=std::chrono::steady_clock::now();
            if(sdk.pump(workspace,web_engine.get(),[&](WindowId from,TabId tab){
                if(from)return detach(from,tab,{float(wx)+70,float(wy)+90});
                return new_window({float(wx)+60,float(wy)+60,config.width*dpi,config.height*dpi});
            })) {sync_views();dirty=true;awake_until=now+std::chrono::milliseconds(200);}
            if(!config.assistant_test.empty()){
                auto& s=workspace.profile->services;auto& state=s.assistant;
                if(assistant_phase==0&&!state.empty()){
                    s.assistant_commands.push_back({{"action","configure"},{"settings",{{"connector","ollama"},{"transport","http-ndjson"},{"model","fixture"},{"endpoint",config.url.substr(0,config.url.size()-1)},{"api_key_env",""},{"executable",""}}}});
                    s.assistant_commands.push_back({{"action","new"}});assistant_phase=1;
                }else if(assistant_phase==1&&state.contains("conversations")&&!state["conversations"].empty()){
                    auto id=state["conversations"].back().value("id","");s.assistant_commands.push_back({{"action","send"},{"conversationId",id},{"text","Bonjour CyUrium"}});assistant_phase=2;
                }else if(assistant_phase==2&&!state["conversations"].empty()){
                    auto& chat=state["conversations"].back();assistant_streamed|=!chat.value("partial","").empty();
                    if(!chat.value("error","").empty())throw std::runtime_error(chat.value("error",""));
                    if(!chat.value("busy",false)&&chat.value("messages",nlohmann::json::array()).size()==2&&initial.active().web.connected&&!initial.active().web.loading){
                        s.assistant_commands.push_back({{"action","attach"},{"conversationId",chat["id"]},{"tabId",initial.selected},{"url",config.url}});assistant_phase=3;
                    }
                }else if(assistant_phase==3&&state.value("attachment",nlohmann::json()).is_object()){
                    auto& chat=state["conversations"].back();s.assistant_commands.push_back({{"action","send"},{"conversationId",chat["id"]},{"text","Explain the attached page"}});assistant_phase=4;
                }else if(assistant_phase==4){
                    auto& chat=state["conversations"].back();
                    if(!chat.value("error","").empty())throw std::runtime_error(chat.value("error",""));
                    assistant_complete=!chat.value("busy",false)&&chat.value("messages",nlohmann::json::array()).size()==4;
                }
                if(!state.value("error","").empty())throw std::runtime_error(state.value("error",""));
            }
            if(session_enabled&&now-checkpoint_at>std::chrono::seconds(1)){if(!sessions.checkpoint(workspace))workspace.profile->services.notice=sessions.error;checkpoint_at=now;}
            if(web_engine&&web_engine->pump(workspace)){
                dirty=true;awake_until=now+std::chrono::milliseconds(150);
                for(const auto& event:web_engine->take_events()) {
                    if(sdk.event(event))continue;
                    if(event.type=="update-ready"){
                        if(begin_update(utf8_path(event.value),utf8_path(SDL_GetBasePath()),profile,workspace.profile->services.notice))done=true;
                        continue;
                    }
                    if(event.type=="extension-tab-close"){workspace.close_tab(workspace.owner(event.tab),event.tab);sync_views();continue;}
                    if(event.type=="new-tab"&&!workspace.owner(event.tab)){
                        if(auto* target=workspace.find(focused)){target->new_tab();target->navigate(event.url);}continue;
                    }
                    auto owner=workspace.owner(event.tab);auto* model=workspace.find(owner);auto it=views.find(owner);
                    if(!model||it==views.end())continue;
                    if(event.type=="new-tab"){model->new_tab();model->navigate(event.url);}
                    if(web_qa&&event.type=="inspection"){
                        web_check(event.message.empty(),event.message.c_str());
                        if(event.request==1){
                            web_check(!event.instance.empty()&&event.value.starts_with("form-"),"HTML/JavaScript fixture did not initialize its form");
                            web_instance=event.instance;web_value=event.value;content_process=event.process;
                            model->open(web_internal_pages[0]);web_internal_tab=model->selected;web_phase=101;web_frame=frames;
                        }else {
                            web_check(event.instance==web_instance&&event.value==web_value,"The tab transfer or history navigation reloaded the document or lost form state");
                            if(event.request==101){
                                web_check(model->selected==web_internal_tab&&web_internal_tab!=web_tab,"Internal page replaced the web tab");
                                workspace.close_tab(owner,web_internal_tab);model->select(web_tab);
                                if(++web_internal_index<std::size(web_internal_pages)){
                                    model->open(web_internal_pages[web_internal_index]);web_internal_tab=model->selected;web_phase=101;
                                }else web_phase=103;
                                web_frame=frames;
                            }else if(event.request==102){model->navigate(config.url+"?page=two");web_phase=2;}
                            else if(event.request==3&&!web_forward_checked){model->forward();web_forward_checked=true;web_phase=31;}
                            else if(event.request==3||event.request==4){
                                auto bounds=it->second->last.bounds;detach(owner,web_tab,{bounds.x+70,bounds.y+90});sync_views();web_frame=frames;web_phase=event.request==3?4:5;
                            }else if(event.request==5){
                                auto bounds=it->second->last.bounds;bounds.x+=50;bounds.y+=60;auto target=new_window(bounds);
                                web_check(workspace.move_tab(owner,target,web_tab,0),"Cross-window attach failed");sync_views();web_frame=frames;web_phase=6;
                            }else if(event.request==6){model->navigate("https://example.org");web_phase=7;}
                        }
                    }
                    if(event.type=="shortcut"){
                        SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.windowID=it->second->native;
                        if(event.key.size()==1)key.key.key=SDL_Keycode(event.key[0]);
                        else if(event.key=="tab")key.key.key=SDLK_TAB;
                        else if(event.key=="left")key.key.key=SDLK_LEFT;
                        else if(event.key=="right")key.key.key=SDLK_RIGHT;
                        else if(event.key=="f5")key.key.key=SDLK_F5;
                        else if(event.key=="f6")key.key.key=SDLK_F6;
                        else if(event.key=="f11")key.key.key=SDLK_F11;
                        key.key.mod=SDL_Keymod((event.control?SDL_KMOD_CTRL:0)|(event.shift?SDL_KMOD_SHIFT:0)|(event.alt?SDL_KMOD_ALT:0));
                        if(auto* native=SDL_GetWindowFromID(it->second->native)){
                            SDL_RaiseWindow(native);
#ifdef _WIN32
                            SetFocus(static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(native),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr)));
#endif
                        }
                        handle(key);key.type=SDL_EVENT_KEY_UP;key.key.mod=SDL_KMOD_NONE;ImGui_ImplSDL3_ProcessEvent(&key);
                    }
                }
            }
            const double elapsed=std::chrono::duration<double>(now-start).count();
            if(web_qa){
                if(elapsed>45)throw std::runtime_error("Web test timed out in phase "+std::to_string(web_phase)+"; "+(workspace.find(workspace.owner(web_tab))?workspace.find(workspace.owner(web_tab))->active().web.error:std::string("tab missing")));
                dirty=true;auto owner=workspace.owner(web_tab);auto* model=workspace.find(owner);
                if(model){const auto& state=model->active().web;
                    if(!state.error.empty()&&web_phase!=8)throw std::runtime_error("Web test phase "+std::to_string(web_phase)+": "+state.error);
                    if(web_phase==101&&frames-web_frame>=6){
                        web_check(model->selected==web_internal_tab&&model->active().current().page==web_internal_pages[web_internal_index]&&!state.connected,"Management tab inherited a web document");
                        web_check(!web_engine->embedded(web_tab,SDL_GetWindowFromID(views.at(owner)->native)),"Original web surface remained visible behind the management page");
                        web_engine->inspect(web_tab,101);web_phase=102;
                    }else if(web_phase==103&&frames-web_frame>=6){
                        web_check(model->selected==web_tab&&web_engine->embedded(web_tab,SDL_GetWindowFromID(views.at(owner)->native)),"Closing management tabs did not restore the original web surface");
                        web_engine->inspect(web_tab,102);web_phase=104;
                    }else if(web_phase==0&&state.connected&&!state.loading&&state.url==config.url&&state.title=="Gecko JS verified"){
                        views.at(owner)->view->show_menu();io.AddFocusEvent(true);web_phase=-1;web_frame=frames;
                    }else if(web_phase==-1&&frames-web_frame>=6){
                        auto& platform_io=ImGui::GetPlatformIO();web_check(platform_io.Viewports.Size>1,"The menu did not create a native overlay above Gecko");
                        auto* data=platform_io.Viewports.back()->DrawData;
                        if(data&&data->DisplaySize.x>0){
                            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command!=nullptr,"Acquire menu capture");
                            ImGui_ImplSDLGPU3_PrepareDrawData(data,command);
                            capture(device,command,data,format,int(data->DisplaySize.x*data->FramebufferScale.x),int(data->DisplaySize.y*data->FramebufferScale.y),config.web_test.parent_path()/"native-menu.bmp");
                        }
                        io.AddKeyEvent(ImGuiKey_Escape,true);web_phase=-2;web_frame=frames;
                    }else if(web_phase==-2&&frames-web_frame>=2){
                        io.AddKeyEvent(ImGuiKey_Escape,false);web_engine->inspect(web_tab,1);web_phase=1;
                    }else if(web_phase==2&&!state.loading&&state.url==config.url+"?page=two"&&state.can_back){model->back();web_phase=3;}
                    else if(web_phase==3&&!state.loading&&state.url==config.url){web_engine->inspect(web_tab,3);web_phase=30;}
                    else if(web_phase==31&&!state.loading&&state.url==config.url+"?page=two"){model->back();web_phase=32;}
                    else if(web_phase==32&&!state.loading&&state.url==config.url){web_engine->inspect(web_tab,3);web_phase=33;}
                    else if(web_phase>=4&&web_phase<=6&&frames-web_frame>=10){
                        auto* native=SDL_GetWindowFromID(views.at(owner)->native);
                        web_check(web_engine->embedded(web_tab,native),"Gecko is not embedded in the destination SDL window");
                        web_engine->inspect(web_tab,web_phase);web_phase*=10;
                    }else if(web_phase==7&&!state.loading&&state.error.empty()&&state.url=="https://example.org/"&&state.title=="Example Domain"){
                        final_web_url=state.url;web_engine->test_crash();web_phase=8;
                    }else if(web_phase==8&&workspace.profile->services.notice.find("redémarré après un plantage")!=std::string::npos&&state.connected&&!state.loading&&state.url==final_web_url&&state.title=="Example Domain"){
                        web_complete=true;done=true;
                    }
                }
            }
            if(config.seconds>0&&elapsed>=config.seconds)break;
            if(save_pending&&now-changed_at>std::chrono::milliseconds(500)&&config.capture.empty()&&!qa){if(!profile_model.save(state_file))SDL_Log("Could not save settings");save_pending=false;}
            const bool editing=std::any_of(views.begin(),views.end(),[](const auto& pair){return pair.second->view->editing();});
            if(!qa&&!dirty&&now>=awake_until&&!drag.tracking()) {
                int timeout=editing?250:1000;
                if(config.seconds>0)timeout=std::min(timeout,std::max(1,int((config.seconds-elapsed)*1000)));
                SDL_Event event;if(SDL_WaitEventTimeout(&event,timeout))handle(event);else if(editing)dirty=true;
            }
            SDL_Event event;while(SDL_PollEvent(&event))handle(event);
            if(done||!workspace.size())break;
            if(!qa&&!dirty&&std::chrono::steady_clock::now()>=awake_until&&!drag.tracking())continue;
            bool visible=false;for(const auto& [_,view]:views) {
                if(!view->native)visible=true;
                else if(auto* native=SDL_GetWindowFromID(view->native))if(!(SDL_GetWindowFlags(native)&SDL_WINDOW_MINIMIZED))visible=true;
            }
            if(!visible&&!qa){
                dirty=false;awake_until=std::chrono::steady_clock::now();
                SDL_Event waiting;if(SDL_WaitEventTimeout(&waiting,500))handle(waiting);continue;
            }
            const auto frame_start=std::chrono::steady_clock::now();
            ImGui_ImplSDLGPU3_NewFrame();ImGui_ImplSDL3_NewFrame();
            if(qa)inject_test();
            ImGui::NewFrame();
            std::vector<WindowDropZone> zones;
            for(auto id:front_to_back)if(auto it=views.find(id);it!=views.end()) {
                auto* native=SDL_GetWindowFromID(it->second->native);
                if(native&&!(SDL_GetWindowFlags(native)&SDL_WINDOW_MINIMIZED))zones.push_back({id,it->second->last.tab_strip,it->second->last.bounds});
            }
            if(auto drop=drag.update({{io.MousePos.x,io.MousePos.y},ImGui::IsMouseClicked(0),ImGui::IsMouseDown(0),ImGui::IsMouseReleased(0),ImGui::IsKeyPressed(ImGuiKey_Escape)},zones)) {
                if(drop->target){if(workspace.move_tab(drop->source,drop->target,drop->tab,drop->before))raise(drop->target);}
                else detach(drop->source,drop->tab,drop->position);
                sync_views();
            }
            if(drag.tracking())if(auto* source=workspace.find(drag.visual().source))source->select(drag.visual().tab);
            for(const auto& [id,view]:views) {
                auto* model=workspace.find(id);if(!model)continue;
                auto* native=SDL_GetWindowFromID(view->native);
                float scale=native?SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(native)):dpi;if(scale<=0)scale=1;
                apply_browser_theme(workspace.profile->theme==Theme::Dark,scale*workspace.profile->ui_scale);
                ViewFrame frame{id,view->placement,scale,view->primary,native&&(SDL_GetWindowFlags(native)&SDL_WINDOW_MAXIMIZED)!=0,view->place,drag.visual()};
                view->last=view->view->draw(*model,frame);view->place=false;
                const auto bounds=view->last.bounds;workspace.placements[id]={bounds.x,bounds.y,bounds.w,bounds.h};
                view->hits={view->last.drag_start,view->last.drag_end,scale*workspace.profile->ui_scale};
                if(native){const auto title=model->active().current().title+" - CyUrium";if(title!=SDL_GetWindowTitle(native))SDL_SetWindowTitle(native,title.c_str());}
            }
            if(drag.visual().active)if(auto* model=workspace.find(drag.visual().source))
                views.at(drag.visual().source)->view->draw_drag_preview(drag.visual(),model->active().current().title,dpi*workspace.profile->ui_scale);
            ImGui::Render();
            const double cost=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count();cpu_sum+=cost;cpu_max=std::max(cpu_max,cost);
            auto* draw=ImGui::GetDrawData();
            if(!(SDL_GetWindowFlags(window)&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED))) {
                auto* command=SDL_AcquireGPUCommandBuffer(device);require(command!=nullptr,"Acquire command buffer");
                SDL_GPUTexture* swapchain=nullptr;Uint32 pixel_width{},pixel_height{};
                require(SDL_WaitAndAcquireGPUSwapchainTexture(command,window,&swapchain,&pixel_width,&pixel_height),"Acquire swapchain");
                if(swapchain&&draw->DisplaySize.x>0&&draw->DisplaySize.y>0) {
                    ImGui_ImplSDLGPU3_PrepareDrawData(draw,command);
                    SDL_GPUColorTargetInfo color{};color.texture=swapchain;color.load_op=SDL_GPU_LOADOP_CLEAR;color.store_op=SDL_GPU_STOREOP_STORE;color.clear_color={1,1,1,1};
                    auto* pass=SDL_BeginGPURenderPass(command,&color,1,nullptr);require(pass!=nullptr,"Begin render pass");ImGui_ImplSDLGPU3_RenderDrawData(draw,command,pass);SDL_EndGPURenderPass(pass);
                    if(assistant_complete){capture(device,command,draw,format,pixel_width,pixel_height,config.assistant_test.parent_path()/"assistant.bmp");done=true;}
                    else if(!config.capture.empty()&&frames>=4){capture(device,command,draw,format,pixel_width,pixel_height,config.capture);done=true;}
                    else require(SDL_SubmitGPUCommandBuffer(command),"Submit frame");
                } else require(SDL_SubmitGPUCommandBuffer(command),"Submit empty frame");
            }
            auto web_placements=[&](){
                std::vector<WebPlacement> result;
                for(const auto& [id,view]:views)if(auto* model=workspace.find(id)) {
                    auto* native=SDL_GetWindowFromID(view->native);auto content=view->last.content;
                    content.x+=6;content.w-=12;content.h-=6;
                    result.push_back({model->selected,native,content,model->active().current().page==Page::Pending&&model->active().web.error.empty()&&(!native||!(SDL_GetWindowFlags(native)&SDL_WINDOW_MINIMIZED)),view->last.address_editing});
                }
                return result;
            };
            if(web_engine)web_engine->sync(workspace,web_placements());
            ImGui::UpdatePlatformWindows();
            for(const auto& [_,view]:views)if(auto* native=native_window(view->last.viewport)) {
                const auto id=SDL_GetWindowID(native);
                if(view->native!=id||(view->primary&&frames==0)) {
                    view->native=id;SDL_SetWindowResizable(native,true);SDL_SetWindowMinimumSize(native,720,520);
                    SDL_SetWindowIcon(native,icon);
                    if(!SDL_SetWindowHitTest(native,hit_test,&view->hits))SDL_SetWindowBordered(native,true);
                }
            }
            ImGui::RenderPlatformWindowsDefault();
            if(web_engine)web_engine->sync(workspace,web_placements());
            if(qa&&test_tick==34) {
                for(const auto& [id,view]:views)if(auto* viewport=ImGui::FindViewportByID(view->last.viewport)) {
                    auto* data=viewport->DrawData;if(!data||data->DisplaySize.x<=0)continue;
                    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command!=nullptr,"Acquire QA capture buffer");
                    ImGui_ImplSDLGPU3_PrepareDrawData(data,command);
                    auto path=config.window_test.parent_path()/(config.window_test.stem().string()+(id==primary?"-first.bmp":"-second.bmp"));
                    capture(device,command,data,format,int(data->DisplaySize.x*data->FramebufferScale.x),
                        int(data->DisplaySize.y*data->FramebufferScale.y),path);
                }
            }
            // Domain mutations are deferred until every view has finished drawing.
            if(session_enabled&&std::any_of(views.begin(),views.end(),[](const auto& pair){const auto& a=pair.second->last;return a.close||a.close_tab||a.quit;}))sessions.checkpoint(workspace);
            const auto ids=workspace.window_ids();
            for(auto id:ids) {
                auto it=views.find(id);if(it==views.end())continue;
                const auto actions=it->second->last;auto* native=SDL_GetWindowFromID(it->second->native);
                if(actions.quit)done=true;
                if(actions.minimize&&native)SDL_MinimizeWindow(native);
                if(actions.maximize&&native){if(SDL_GetWindowFlags(native)&SDL_WINDOW_MAXIMIZED)SDL_RestoreWindow(native);else SDL_MaximizeWindow(native);}
                if(actions.save){save_pending=true;changed_at=std::chrono::steady_clock::now();awake_until=changed_at+std::chrono::milliseconds(650);}
                if(actions.new_window){auto b=actions.bounds;b.x+=48;b.y+=56;new_window(b);}
                if(actions.detach_tab)detach(id,actions.detach_tab,{actions.bounds.x+60,actions.bounds.y+80});
                if(actions.close_tab)workspace.close_tab(id,actions.close_tab);
                if(actions.close)workspace.close_window(id);
            }
            sync_views();++frames;dirty=frames<5;
            if(!config.update_health.empty()&&frames>=5&&config.update_health.parent_path()==profile&&web_engine&&web_engine->ready()&&workspace.profile->services.state.value("ready",false)&&!std::filesystem::exists(config.update_health)){
                std::ofstream health(config.update_health);health<<"{\"started\":true,\"version\":\"" CYURIUM_VERSION "\"}";
            }
        }
        if(web_qa){
            web_check(web_complete,"Web scenario did not complete");
            std::filesystem::create_directories(config.web_test.parent_path());
            nlohmann::json report={{"passed",true},{"engine","Gecko"},{"version",web_engine->version()},{"https",final_web_url},
                {"content_process",content_process},{"checks",{"HTML and JavaScript","native menu above web content","management pages use separate tabs","original web surface hidden while management tab is selected","original document and form preserved after closing management tabs","form state","back/forward with document state","native child embedding","detach first window","detach last tab of a secondary window","attach without reload","real HTTPS navigation","automatic recovery after actual engine termination"}}};
            std::ofstream(config.web_test)<<report.dump(2)<<'\n';
            auto snapshot=profile/"gecko-appdata/web-content.png";
            if(std::filesystem::exists(snapshot))std::filesystem::copy_file(snapshot,config.web_test.parent_path()/"web-content.png",std::filesystem::copy_options::overwrite_existing);
        }
        if(!config.assistant_test.empty()){
            if(!assistant_complete||!assistant_streamed)throw std::runtime_error("Native conversation scenario did not finish with streamed text");
            nlohmann::json report={{"passed",true},{"provider","existing CyUEMCPSDK Ollama transport / local fixture"},{"checks",{"native conversation commands","progressive response","second turn in same provider session","explicit real Gecko page attachment","native panel rendered beside Gecko","external MCP remains disabled"}}};
            std::ofstream(config.assistant_test)<<report.dump(2)<<'\n';
        }
        if(qa){
            test_check(test_complete,"test did not reach completion");
            if(!config.window_test.parent_path().empty())std::filesystem::create_directories(config.window_test.parent_path());
            std::ofstream report(config.window_test);report<<"{\n  \"passed\": true,\n  \"backend\": \""<<SDL_GetGPUDeviceDriver(device)<<"\",\n  \"checks\": [\"drag reorder\", \"detach to distinct native window\", \"cross-window attach\", \"preserved identity and history\", \"close primary while secondary survives\", \"close last window\"]\n}\n";
            if(!report)throw std::runtime_error("Could not write native window test report");
        }
        const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if(!config.metrics.empty()) {
            if(!config.metrics.parent_path().empty())std::filesystem::create_directories(config.metrics.parent_path());
            std::ofstream report(config.metrics);
            report<<"{\n  \"scope\": \"native shell process only; excludes Gecko child processes\",\n"
                <<"  \"browser_windows\": "<<workspace.size()<<",\n  \"session_recovered\": "<<(workspace.profile->services.recovered_session?"true":"false")<<",\n"
                <<"  \"gpu_backend\": \""<<SDL_GetGPUDeviceDriver(device)<<"\",\n"
                <<"  \"elapsed_seconds\": "<<elapsed<<",\n  \"rendered_frames\": "<<frames
                <<",\n  \"ui_build_mean_ms\": "<<(frames?cpu_sum/frames:0)<<",\n  \"ui_build_max_ms\": "<<cpu_max
                <<",\n  \"process_cpu_ms\": "<<(cpu_seconds()-process_start)*1000
                <<",\n  \"resident_memory_mb\": "<<resident_mb()<<"\n}\n";
            if(!report)throw std::runtime_error("Could not write metrics");
        }
        if(config.capture.empty()&&!qa&&!profile_model.save(state_file))SDL_Log("Could not save settings on exit");
        if(session_enabled&&!sessions.checkpoint(workspace,true))SDL_Log("Could not mark session clean");
        sdk.stop();web_engine.reset();views.clear();SDL_WaitForGPUIdle(device);ImGui_ImplSDLGPU3_Shutdown();renderer=false;
        ImGui_ImplSDL3_Shutdown();platform=false;ImGui::DestroyContext();context=false;
        set_browser_logo({});SDL_ReleaseGPUTexture(device,logo);logo=nullptr;SDL_DestroySurface(icon);icon=nullptr;
        SDL_ReleaseWindowFromGPUDevice(device,window);SDL_DestroyGPUDevice(device);device=nullptr;
        SDL_DestroyWindow(window);window=nullptr;SDL_Quit();return 0;
    } catch(const std::exception& error) {
        web_engine.reset();
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"%s",error.what());
        std::ofstream("cyurium-error.log",std::ios::app)<<error.what()<<'\n';
        if(renderer){SDL_WaitForGPUIdle(device);ImGui_ImplSDLGPU3_Shutdown();}
        if(platform)ImGui_ImplSDL3_Shutdown();if(context)ImGui::DestroyContext();
        if(icon)SDL_DestroySurface(icon);
        if(device){if(logo)SDL_ReleaseGPUTexture(device,logo);if(window)SDL_ReleaseWindowFromGPUDevice(device,window);SDL_DestroyGPUDevice(device);}
        if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
