// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "updater.hpp"
#include "json.hpp"
#include <fstream>
#include <set>
#include <vector>
#include <random>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#endif
namespace cyurium {
namespace {
std::string utf8(const std::filesystem::path& p){auto v=p.u8string();return {v.begin(),v.end()};}
std::filesystem::path path(const char* s){return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s)));}
#ifdef _WIN32
std::wstring quote(std::wstring_view s){std::wstring r=L"\"";std::size_t n=0;for(auto c:s){if(c==L'\\'){++n;continue;}r.append(c==L'"'?n*2+1:n,L'\\');n=0;r+=c;}r.append(n*2,L'\\');return r+L'"';}
bool child_path(const std::filesystem::path& p,const std::filesystem::path& root){auto a=std::filesystem::weakly_canonical(p).wstring(),b=std::filesystem::weakly_canonical(root).wstring()+L"\\";return a.size()>b.size()&&_wcsnicmp(a.c_str(),b.c_str(),b.size())==0;}
std::string hash(const std::filesystem::path& file){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE handle=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 unavailable");
    DWORD size=0,read=0;BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&read,0);std::vector<unsigned char> object(size),digest(32);
    if(BCryptCreateHash(algorithm,&handle,object.data(),size,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SHA256 initialization failed");}
    std::ifstream input(file,std::ios::binary);char buffer[65536];
    while(input){input.read(buffer,sizeof(buffer));if(BCryptHashData(handle,reinterpret_cast<PUCHAR>(buffer),ULONG(input.gcount()),0)<0){BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SHA256 failed");}}
    const bool valid=input.eof();BCryptFinishHash(handle,digest.data(),DWORD(digest.size()),0);BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(algorithm,0);
    if(!valid)throw std::runtime_error("Package file unreadable");constexpr char hex[]="0123456789abcdef";std::string result;for(auto c:digest){result+=hex[c>>4];result+=hex[c&15];}return result;
}
void verify(const std::filesystem::path& stage){
    auto manifest=stage/"package-files.json";if(!std::filesystem::exists(manifest)||std::filesystem::file_size(manifest)>16*1024*1024)throw std::runtime_error("Package manifest missing");
    std::ifstream file(manifest);auto files=nlohmann::json::parse(file);if(!files.is_object()||files.empty()||files.size()>50000)throw std::runtime_error("Invalid package manifest");
    std::set<std::filesystem::path> expected;
    for(auto it=files.begin();it!=files.end();++it){auto name=path(it.key().c_str());auto target=stage/name;
        if(name.is_absolute()||!child_path(target,stage)||it.key().find(':')!=std::string::npos||it.key().find('\\')!=std::string::npos)throw std::runtime_error("Invalid package path");
        if(!std::filesystem::is_regular_file(target)||hash(target)!=it.value().get<std::string>())throw std::runtime_error("Package integrity check failed: "+it.key());expected.insert(std::filesystem::weakly_canonical(target));
    }
    for(auto& entry:std::filesystem::recursive_directory_iterator(stage)){
        if(GetFileAttributesW(entry.path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Package contains a link");
        if(entry.is_regular_file()&&entry.path()!=manifest&&!expected.contains(std::filesystem::weakly_canonical(entry.path())))throw std::runtime_error("Unexpected package file");
    }
    if(!files.contains("CyUrium.exe")||!files.contains("engine/app/application.ini")||!files.contains("assets/branding/urium-logo.rgba"))throw std::runtime_error("Incomplete browser package");
}
HANDLE launch(std::wstring command,const std::filesystem::path& cwd){STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,0,nullptr,cwd.c_str(),&startup,&child))throw std::runtime_error("Cannot start browser: "+std::to_string(GetLastError()));CloseHandle(child.hThread);return child.hProcess;}
#endif
}
bool begin_update(const std::filesystem::path& stage,const std::filesystem::path& install,const std::filesystem::path& profile,std::string& error){
#ifdef _WIN32
    try {
        if(!child_path(stage,profile/"gecko-appdata/updates"))throw std::runtime_error("Update staging path rejected");verify(stage);
        auto helper=profile/"update-helper.exe";std::filesystem::copy_file(install/"CyUrium.exe",helper,std::filesystem::copy_options::overwrite_existing);
        std::random_device random;const auto nonce=std::to_wstring(random())+std::to_wstring(random());
        auto command=quote(helper.wstring())+L" --apply-update "+quote(stage.wstring())+L" "+quote(install.wstring())+L" "+quote(profile.wstring())+L" "+std::to_wstring(GetCurrentProcessId())+L" "+nonce;
        auto process=launch(command,profile);CloseHandle(process);return true;
    }catch(const std::exception& e){error=e.what();return false;}
#else
    error="Mises à jour natives disponibles sous Windows uniquement.";return false;
#endif
}
int run_update_helper(int argc,char** argv){
    if(argc<2||std::string_view(argv[1])!="--apply-update")return -1;
#ifdef _WIN32
    if(argc!=7)return 2;
    const auto stage=std::filesystem::absolute(path(argv[2])),install=std::filesystem::absolute(path(argv[3])),profile=std::filesystem::absolute(path(argv[4]));
    std::filesystem::path candidate,backup;bool moved=false,replaced=false;
    try {
        if(!child_path(stage,profile/"gecko-appdata/updates")||install==install.root_path()||child_path(profile,install)||!std::filesystem::is_regular_file(install/"engine/app/application.ini"))throw std::runtime_error("Unsafe update destination");
        verify(stage);
        const auto parent=OpenProcess(SYNCHRONIZE,FALSE,std::stoul(argv[5]));if(parent){const auto result=WaitForSingleObject(parent,60000);CloseHandle(parent);if(result!=WAIT_OBJECT_0)throw std::runtime_error("Browser still running");}
        auto suffix=path(argv[6]);if(suffix.wstring().find_first_not_of(L"0123456789")!=std::wstring::npos)throw std::runtime_error("Invalid update nonce");
        candidate=install.parent_path()/(install.filename().wstring()+L".incoming-"+suffix.wstring());backup=install.parent_path()/(install.filename().wstring()+L".previous-"+suffix.wstring());
        if(std::filesystem::exists(candidate)||std::filesystem::exists(backup))throw std::runtime_error("Update destination already exists");
        std::filesystem::copy(stage,candidate,std::filesystem::copy_options::recursive);verify(candidate);
        std::filesystem::rename(install,backup);moved=true;
        std::filesystem::rename(candidate,install);replaced=true;
        auto health=profile/(L"update-health-"+suffix.wstring()+L".json");
        const auto command=quote((install/"CyUrium.exe").wstring())+L" --profile "+quote(profile.wstring())+L" --update-health "+quote(health.wstring());
        auto child=launch(command,profile);bool healthy=false;
        for(int i=0;i<150;++i){const auto status=WaitForSingleObject(child,200);if(std::filesystem::exists(health)){healthy=true;break;}if(status==WAIT_OBJECT_0)break;}
        if(!healthy){TerminateProcess(child,1);WaitForSingleObject(child,5000);CloseHandle(child);throw std::runtime_error("Updated browser failed its startup check");}
        CloseHandle(child);std::ofstream(profile/"update-result.json")<<nlohmann::json({{"success",true},{"backup",utf8(backup)}}).dump();return 0;
    }catch(const std::exception& e){
        std::error_code ec;
        if(moved){if(replaced&&std::filesystem::exists(install)){auto failed=install;failed+=L".failed";if(!std::filesystem::exists(failed))std::filesystem::rename(install,failed,ec);}if(!std::filesystem::exists(install))std::filesystem::rename(backup,install,ec);}
        std::ofstream(profile/"update-result.json")<<nlohmann::json({{"success",false},{"error",e.what()}}).dump();
        if(moved&&std::filesystem::exists(install/"CyUrium.exe"))try{auto child=launch(quote((install/"CyUrium.exe").wstring())+L" --profile "+quote(profile.wstring()),profile);CloseHandle(child);}catch(...){}
        return 1;
    }
#else
    return 2;
#endif
}
}
