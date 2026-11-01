// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "bridge_channel.hpp"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <mutex>
#include <thread>
using namespace cyurium;
using Json=nlohmann::json;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Peer {
    SOCKET socket=INVALID_SOCKET;
    explicit Peer(std::uint16_t port){socket=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);DWORD timeout=3500;setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);check(connect(socket,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0,"connect failed");}
    ~Peer(){closesocket(socket);}
    void send(const Json& value){auto text=value.dump()+"\n";check(::send(socket,text.data(),int(text.size()),0)==int(text.size()),"send failed");}
    Json read(){std::string data;char c;while(recv(socket,&c,1,0)==1){if(c=='\n')return Json::parse(data);data+=c;}throw std::runtime_error("peer closed or timed out");}
    bool closed(){char c;return recv(socket,&c,1,0)==0;}
};
int main(){try{
    check(bridge_hmac("key","The quick brown fox jumps over the lazy dog")=="f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8","HMAC known vector mismatch");
    std::mutex mutex;std::vector<Json> messages;LocalBridge bridge([&](Json m){std::lock_guard lock(mutex);messages.push_back(std::move(m));});bridge.start(GetCurrentProcessId());
    bridge.send({{"type","private-command"},{"value","must not leak before authentication"}});
    Json previous;
    auto hello=[&](const Json& challenge){Json h={{"type","hello"},{"protocol",1},{"nonce",bridge_random(32)},{"pid",std::uint32_t(GetCurrentProcessId())},{"version","153.6.0"},{"remote",true},{"fission",true}};h["proof"]=bridge_hmac(bridge.secret(),"client\n"+bridge_transcript(challenge.at("nonce").get<std::string>(),h));return h;};
    {
        Peer p(bridge.port());auto challenge=p.read();check(challenge.at("type")=="challenge"&&challenge.dump().find(bridge.secret())==std::string::npos,"secret appeared on wire");
        fd_set fds;FD_ZERO(&fds);FD_SET(p.socket,&fds);timeval wait{0,150000};check(select(0,&fds,nullptr,nullptr,&wait)==0,"data leaked before authentication");
        auto h=hello(challenge);previous=h;h["proof"]=std::string(64,'0');p.send(h);check(p.closed(),"wrong HMAC accepted");
    }
    {Peer p(bridge.port());p.read();p.send(previous);check(p.closed(),"old proof replay accepted");}
    {Peer p(bridge.port());p.read();p.send({{"type","state"}});check(p.closed(),"command accepted before authentication");}
    {Peer p(bridge.port());p.read();std::string deep(80,'[');deep+="0";deep+=std::string(80,']');deep+='\n';::send(p.socket,deep.data(),int(deep.size()),0);check(p.closed(),"deep JSON accepted");}
    {Peer p(bridge.port());p.read();std::string large(8500,'x');::send(p.socket,large.data(),int(large.size()),0);char c;check(recv(p.socket,&c,1,0)<=0,"oversized hello accepted");}
    {Peer p(bridge.port());p.read();check(p.closed(),"silent client held bridge indefinitely");}
    {
        Peer p(bridge.port());auto challenge=p.read();auto h=hello(challenge);p.send(h);
        auto accepted=p.read();check(accepted.at("type")=="authenticated"&&accepted.at("proof")==bridge_hmac(bridge.secret(),"server\n"+bridge_transcript(challenge.at("nonce").get<std::string>(),h)),"server authentication failed");
        check(p.read().at("type")=="private-command","queued command lost");p.send({{"type","state"},{"value","received"}});
        std::this_thread::sleep_for(std::chrono::milliseconds(100));std::lock_guard lock(mutex);
        check(messages.size()==2&&messages[0].at("type")=="hello"&&!messages[0].contains("proof")&&messages[1].at("value")=="received","authenticated messages not isolated");
    }
    std::cout<<"PASS: system randomness, HMAC vector, mutual authentication, replay, pre-auth data isolation, deep/large JSON, timeout and valid reconnect\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
