// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "bridge_channel.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <span>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace cyurium {
using Json=nlohmann::json;
namespace {
constexpr std::size_t max_frame=8*1024*1024,max_queue=16*1024*1024;
std::string hex(std::span<const unsigned char> bytes){constexpr char digits[]="0123456789abcdef";std::string out;for(auto c:bytes){out+=digits[c>>4];out+=digits[c&15];}return out;}
bool hex64(const Json& v){if(!v.is_string())return false;const auto& s=v.get_ref<const std::string&>();return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool equal(std::string_view a,std::string_view b){if(a.size()!=b.size())return false;unsigned diff=0;for(std::size_t i=0;i<a.size();++i)diff|=static_cast<unsigned char>(a[i]^b[i]);return diff==0;}
Json parse(std::string_view text,unsigned limit){
    unsigned depth=0;bool quoted=false,escape=false;
    for(char c:text){if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;else if(c=='{'||c=='['){if(++depth>limit)throw std::runtime_error("Bridge nesting limit");}else if(c=='}'||c==']'){if(!depth)throw std::runtime_error("Bridge syntax");--depth;}}
    auto value=Json::parse(text,nullptr,false);if(!value.is_object())throw std::runtime_error("Invalid bridge frame");return value;
}
}
std::string bridge_random(std::size_t count){std::vector<unsigned char> bytes(count);if(BCryptGenRandom(nullptr,bytes.data(),ULONG(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("System random generator unavailable");return hex(bytes);}
std::string bridge_hmac(std::string_view secret,std::string_view text){
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG)<0)throw std::runtime_error("HMAC unavailable");
    std::array<unsigned char,32> digest{};
    const auto status=BCryptHash(algorithm,reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),ULONG(secret.size()),reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),ULONG(text.size()),digest.data(),ULONG(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm,0);if(status<0)throw std::runtime_error("HMAC failed");return hex(digest);
}
std::string bridge_transcript(std::string_view challenge,const Json& h){
    if(h.value("type","")!="hello"||h.value("protocol",0)!=1||!hex64(h.at("nonce"))||!h.at("pid").is_number_unsigned()||!h.at("remote").is_boolean()||!h.at("fission").is_boolean())throw std::runtime_error("Invalid bridge hello");
    const auto version=h.at("version").get<std::string>();if(version.empty()||version.size()>100||version.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-")!=std::string::npos)throw std::runtime_error("Invalid engine version");
    return "cyurium-bridge-v1\n"+std::string(challenge)+"\n"+h.at("nonce").get<std::string>()+"\n"+std::to_string(h.at("pid").get<std::uint32_t>())+"\n"+version+"\n"+(h.at("remote").get<bool>()?"1":"0")+"\n"+(h.at("fission").get<bool>()?"1":"0");
}
struct LocalBridge::Impl {
    std::function<void(Json)> receive;
    SOCKET listener=INVALID_SOCKET,client=INVALID_SOCKET;
    std::string secret=bridge_random(32);std::uint16_t port=0;std::uint32_t pid=0;
    bool winsock=false;std::jthread worker;std::mutex mutex;std::deque<std::string> outgoing;std::size_t queued=0;
    void run(std::stop_token stop){
        bool authenticated=false;std::string buffer,pending,challenge;auto accepted=std::chrono::steady_clock::now();
        auto reject=[&]{closesocket(client);client=INVALID_SOCKET;buffer.clear();pending.clear();authenticated=false;};
        try{while(!stop.stop_requested()){
            if(client!=INVALID_SOCKET&&!authenticated&&std::chrono::steady_clock::now()-accepted>std::chrono::seconds(2)){reject();continue;}
            fd_set read,write;FD_ZERO(&read);FD_ZERO(&write);const auto socket=client==INVALID_SOCKET?listener:client;FD_SET(socket,&read);
            if(client!=INVALID_SOCKET){
                if(authenticated&&pending.empty()){std::lock_guard lock(mutex);if(!outgoing.empty()){pending=std::move(outgoing.front());outgoing.pop_front();queued-=pending.size();}}
                if(!pending.empty())FD_SET(client,&write);
            }
            timeval timeout{0,50000};const auto selected=select(0,&read,&write,nullptr,&timeout);if(selected==SOCKET_ERROR)throw std::runtime_error("Bridge socket failed");
            if(client==INVALID_SOCKET){if(FD_ISSET(listener,&read)){
                client=accept(listener,nullptr,nullptr);if(client!=INVALID_SOCKET){u_long mode=1;ioctlsocket(client,FIONBIO,&mode);accepted=std::chrono::steady_clock::now();challenge=bridge_random(32);pending=Json({{"type","challenge"},{"protocol",1},{"nonce",challenge}}).dump()+"\n";}
            }continue;}
            if(FD_ISSET(client,&write)){
                const auto sent=::send(client,pending.data(),int(std::min<std::size_t>(pending.size(),65536)),0);
                if(sent>0)pending.erase(0,sent);else if(WSAGetLastError()!=WSAEWOULDBLOCK){if(authenticated)throw std::runtime_error("Bridge disconnected");reject();continue;}
            }
            if(!FD_ISSET(client,&read))continue;
            char bytes[16384];const auto count=recv(client,bytes,sizeof(bytes),0);
            if(count<=0){if(count<0&&WSAGetLastError()==WSAEWOULDBLOCK)continue;if(authenticated)throw std::runtime_error("Bridge disconnected");reject();continue;}
            buffer.append(bytes,count);if(buffer.size()>(authenticated?max_frame:8192)){if(authenticated)throw std::runtime_error("Bridge frame too large");reject();continue;}
            std::size_t end;
            while((end=buffer.find('\n'))!=std::string::npos){
                auto line=buffer.substr(0,end);buffer.erase(0,end+1);
                try{
                    auto message=parse(line,authenticated?64:8);
                    if(!authenticated){
                        auto transcript=bridge_transcript(challenge,message);
                        if(message.at("pid").get<std::uint32_t>()!=pid||!hex64(message.at("proof"))||!equal(message.at("proof").get_ref<const std::string&>(),bridge_hmac(secret,"client\n"+transcript)))throw std::runtime_error("Bridge authentication failed");
                        pending+=Json({{"type","authenticated"},{"protocol",1},{"proof",bridge_hmac(secret,"server\n"+transcript)}}).dump()+"\n";
                        authenticated=true;message.erase("proof");message.erase("nonce");receive(std::move(message));
                    }else{
                        if(message.value("type","")=="hello"||message.contains("token"))throw std::runtime_error("Unexpected bridge authentication frame");
                        receive(std::move(message));
                    }
                }catch(...){if(authenticated)throw;reject();break;}
            }
        }}catch(...){if(!stop.stop_requested())receive({{"type","disconnected"}});}
    }
    ~Impl(){worker.request_stop();if(worker.joinable())worker.join();if(client!=INVALID_SOCKET)closesocket(client);if(listener!=INVALID_SOCKET)closesocket(listener);if(winsock)WSACleanup();SecureZeroMemory(secret.data(),secret.size());}
};
LocalBridge::LocalBridge(std::function<void(Json)> receive):impl_(std::make_unique<Impl>()){
    auto& p=*impl_;p.receive=std::move(receive);WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("Bridge Winsock initialization failed");p.winsock=true;
    p.listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(p.listener==INVALID_SOCKET)throw std::runtime_error("Bridge socket creation failed");
    BOOL exclusive=TRUE;if(setsockopt(p.listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<char*>(&exclusive),sizeof(exclusive)))throw std::runtime_error("Exclusive bridge socket unavailable");
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(p.listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))||listen(p.listener,4))throw std::runtime_error("Bridge bind failed");
    int size=sizeof(address);if(getsockname(p.listener,reinterpret_cast<sockaddr*>(&address),&size))throw std::runtime_error("Bridge port unavailable");p.port=ntohs(address.sin_port);
}
LocalBridge::~LocalBridge()=default;
std::uint16_t LocalBridge::port()const{return impl_->port;}
const std::string& LocalBridge::secret()const{return impl_->secret;}
void LocalBridge::start(std::uint32_t process_id){impl_->pid=process_id;impl_->worker=std::jthread([this](std::stop_token stop){impl_->run(stop);});}
void LocalBridge::send(Json message){
    auto line=message.dump()+"\n";std::lock_guard lock(impl_->mutex);
    if(line.size()>max_frame||impl_->queued+line.size()>max_queue||impl_->outgoing.size()>=2048)throw std::runtime_error("Bridge output queue full");
    impl_->queued+=line.size();impl_->outgoing.push_back(std::move(line));
}
}
