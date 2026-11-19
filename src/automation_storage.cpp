// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "automation.hpp"
#include "session_protection.hpp"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <dpapi.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace cyurium::automation {
namespace {
// Serialize first pairing across CLI processes without replacing another caller's identity.
struct CredentialLock {
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
    explicit CredentialLock(const std::filesystem::path& file){
        auto end=Clock::now()+std::chrono::seconds(3);
        do{handle=CreateFileW(file.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);if(handle!=INVALID_HANDLE_VALUE)return;auto error=GetLastError();if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_ACCESS_DENIED)throw Error("StorageError","Cannot lock client identity");std::this_thread::sleep_for(std::chrono::milliseconds(10));}while(Clock::now()<end);
        throw Error("Busy","Another process is pairing this client");
    }
    ~CredentialLock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
#else
    int handle=-1;
    explicit CredentialLock(const std::filesystem::path& file){handle=open(file.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);if(handle<0)throw Error("StorageError","Cannot lock client identity");auto end=Clock::now()+std::chrono::seconds(3);while(flock(handle,LOCK_EX|LOCK_NB)){if(Clock::now()>=end){close(handle);handle=-1;throw Error("Busy","Another process is pairing this client");}std::this_thread::sleep_for(std::chrono::milliseconds(10));}}
    ~CredentialLock(){if(handle>=0)close(handle);}
#endif
};
}
Json load_private(const std::filesystem::path& path,Json fallback){
    if(!std::filesystem::exists(path))return fallback;
    if(std::filesystem::file_size(path)>32*1024*1024)throw Error("QuotaExceeded","Private record exceeds its size limit");
    std::ifstream stream(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(stream)),{});
    if(bytes.starts_with("CYURIUM-DPAPI-1\n")){bool legacy=false;return parse_json(unprotect_session(bytes,legacy),16*1024*1024);}
#ifdef _WIN32
    // CyTools-era conversation/discovery records used raw account DPAPI blobs.
    DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};
    if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))throw Error("StorageError","Saved data cannot be read by this OS account");
    std::string plain(reinterpret_cast<char*>(output.pbData),output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return parse_json(plain,16*1024*1024);
#else
    return parse_json(bytes,16*1024*1024);
#endif
}
void save_private(const std::filesystem::path& path,const Json& value){
    private_directory(path.parent_path());auto temporary=path;temporary+=".new";auto bytes=protect_session(value.dump());
    {auto file=open_private_output_file(temporary);if(std::fwrite(bytes.data(),1,bytes.size(),file.get())!=bytes.size()||std::fflush(file.get()))throw Error("StorageError","Cannot save private data");}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw Error("StorageError","Cannot replace private data");
#else
    std::filesystem::rename(temporary,path);
#endif
}
Json rpc(const Json& connection,const Json& request,const Cancellation& cancel){
    const auto url=connection.at("url").get<std::string>();auto parsed=parse_url(url);
    if(parsed.scheme!="http"||parsed.host!="127.0.0.1"||parsed.target!="/v1")throw Error("AuthenticationRequired","Invalid instance discovery endpoint");
    HttpRequest http;http.method="POST";http.url=url;http.body=request.dump();http.timeout=std::chrono::seconds(120);http.headers={{"Authorization","Bearer "+connection.at("secret").get<std::string>()},{"Content-Type","application/json"}};
    auto response=HttpClient().perform(http,cancel);
    if(!response.body.empty()){auto result=parse_json(response.body,16*1024*1024);if(response.status==200)return result;if(result.contains("error")){auto e=result["error"];throw Error(e.value("code","InstanceError"),e.value("message","Instance operation failed"));}}
    if(response.status==401||response.status==403)throw Error("AccessDenied","Instance access denied");
    throw Error("ConnectionFailed","Instance request failed (HTTP "+std::to_string(response.status)+")");
}
Json pair(const std::filesystem::path& profile,const std::string& client_name,const std::string& legacy_token_env){
    if(client_name.empty()||client_name.size()>80||client_name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=client_name.npos)throw Error("InvalidArguments","Client name must use ASCII letters, digits, underscores, dots or hyphens");
    auto root=load_private(profile/"sdk/connection.bin");if(!root.contains("secret"))throw Error("InstanceRequired","Enable automation in the selected running browser instance");
    auto file=profile/"sdk/clients"/(client_name+".bin");Json credentials;
    {private_directory(file.parent_path());CredentialLock guard(file.parent_path()/(client_name+".lock"));credentials=load_private(file);if(!credentials.contains("key")){credentials={{"key",random_token()}};save_private(file,credentials);}}
    Json request={{"method","pair"},{"key",credentials["key"]}};
    if(!legacy_token_env.empty()){
        if(legacy_token_env.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=legacy_token_env.npos)throw Error("InvalidArguments","Expected a credential environment variable name");
        const char* token=std::getenv(legacy_token_env.c_str());if(!token)throw Error("AuthenticationRequired","Legacy credential environment variable is not set");request["legacyToken"]=token;
    }
    return rpc(root,request);
}
}
