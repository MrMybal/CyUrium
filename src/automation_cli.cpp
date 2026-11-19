// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "automation.hpp"
#include "../third_party/cymcpsdk/src/mcp_headers.hpp"
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
using namespace cyurium::automation;
namespace {
std::string read_file(const std::filesystem::path& path){std::ifstream stream(path,std::ios::binary);if(!stream)throw Error("InvalidArguments","Cannot open JSON arguments file");return {(std::istreambuf_iterator<char>(stream)),{}};}
int stdio_proxy(const Json& connection){
    std::mutex output,session_mutex;std::string session,protocol;cymcp::Cancellation lifetime;
    auto write=[&](const Json& response){std::lock_guard lock(output);std::cout<<response.dump()<<'\n'<<std::flush;};
    auto call=[&](Json request){
        const auto id=request.value("id",Json());
        try{
            HttpRequest http;http.method="POST";auto url=connection.at("url").get<std::string>();http.url=url.substr(0,url.size()-3)+"/mcp";http.body=request.dump();http.timeout=std::chrono::milliseconds(0);http.idle_timeout=std::chrono::minutes(10);
            http.headers={{"Authorization","Bearer "+connection.at("secret").get<std::string>()},{"Content-Type","application/json"},{"Accept","application/json, text/event-stream"}};
            const auto method=request.value("method","");
            {std::lock_guard lock(session_mutex);if(!session.empty())http.headers["Mcp-Session-Id"]=session;if(!protocol.empty()&&method!="initialize")http.headers["MCP-Protocol-Version"]=protocol;}
            auto params=request.value("params",Json::object());auto metadata=params.value("_meta",Json::object());
            if(metadata.contains(cymcp::mcp_http::version_key)){
                http.headers["MCP-Protocol-Version"]=metadata.at(cymcp::mcp_http::version_key);http.headers["Mcp-Method"]=method;
                if(auto field=cymcp::mcp_http::name_field(method);field&&params.contains(field))http.headers["Mcp-Name"]=cymcp::mcp_http::header_value(params.at(field).get<std::string>());
            }
            HttpResponse response;
            if(method=="subscriptions/listen"){
                std::string buffer;response=HttpClient().perform(http,lifetime,[&](const char* bytes,std::size_t count){buffer.append(bytes,count);for(auto pos=buffer.find('\n');pos!=buffer.npos;pos=buffer.find('\n')){auto line=buffer.substr(0,pos);buffer.erase(0,pos+1);if(line.starts_with("data:")){auto data=line.substr(5);try{write(parse_json(data));}catch(...){}}}return !lifetime.cancelled();});
            }else response=HttpClient().perform(http,lifetime);
            if(response.status==202)return;
            if(response.status!=200)throw Error("ConnectionFailed","MCP instance returned HTTP "+std::to_string(response.status));
            if(response.body.empty())return;
            auto data=parse_json(response.body,16*1024*1024);
            if(method=="initialize"&&data.contains("result")){
                std::lock_guard lock(session_mutex);protocol=data["result"].value("protocolVersion","");for(auto& [name,value]:response.headers){auto lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});if(lower=="mcp-session-id")session=value;}
            }
            write(data);
        }catch(const Error& e){if(!id.is_null())write({{"jsonrpc","2.0"},{"id",id},{"error",{{"code",-32000},{"message",redact_error_message(e.what())},{"data",{{"code",e.code}}}}}});}
    };
    {Executor workers(4,64);std::string line;while(std::getline(std::cin,line)){
        auto request=parse_json(line,4*1024*1024);const auto method=request.value("method","");
        if(method=="initialize"||method=="notifications/cancelled")call(std::move(request));else workers.post([&,request=std::move(request)]{call(request);});
    }lifetime.cancel();}
    return 0;
}
}
int main(int argc,char** argv){
    try{
        std::vector<std::string> args;
#ifdef _WIN32
        int count=0;auto wide=CommandLineToArgvW(GetCommandLineW(),&count);if(!wide)throw Error("InvalidArguments","Cannot read command line");
        for(int i=1;i<count;++i){int bytes=WideCharToMultiByte(CP_UTF8,0,wide[i],-1,nullptr,0,nullptr,nullptr);std::string value(std::size_t(bytes),0);WideCharToMultiByte(CP_UTF8,0,wide[i],-1,value.data(),bytes,nullptr,nullptr);value.pop_back();args.push_back(std::move(value));}LocalFree(wide);
#else
        for(int i=1;i<argc;++i)args.emplace_back(argv[i]);
#endif
        auto profile=default_profile();std::string client="terminal",json="{}",legacy_env;bool offline=false,control=false,external=false,help=false;std::vector<std::string> positional;
        for(std::size_t i=0;i<args.size();++i){auto value=[&](){if(++i>=args.size())throw Error("InvalidArguments","Option requires a value");return args[i];};
            if(args[i]=="--profile"||args[i]=="--instance")profile=std::filesystem::absolute(std::filesystem::u8path(value()));
            else if(args[i]=="--client")client=value();else if(args[i]=="--legacy-token-env")legacy_env=value();else if(args[i]=="--args")json=value();else if(args[i]=="--args-file")json=read_file(std::filesystem::u8path(value()));
            else if(args[i]=="--offline")offline=true;else if(args[i]=="--control")control=true;else if(args[i]=="--external")external=true;else if(args[i]=="--json"){}else if(args[i]=="--help"||args[i]=="-h")help=true;
            else if(args[i].starts_with("--"))throw Error("InvalidArguments","Unknown option: "+args[i]);else positional.push_back(args[i]);
        }
        if(help||positional.empty()){
            std::cout<<"CyUrium native CLI · CyMCPSDK "<<version<<"\n"
                "Usage: CyUriumCLI [--profile DIR] [--client NAME] [--offline] COMMAND\n"
                "  list                         List operations (live instance when available)\n"
                "  describe NAME                Show full operation schemas\n"
                "  invoke NAME --args JSON      Invoke a real browser operation\n"
                "              --args-file FILE Read arguments from a UTF-8 JSON file\n"
                "  jobs submit NAME --args JSON Submit a durable job, once\n"
                "  jobs list | get ID | cancel ID | delete ID\n"
                "  --legacy-token-env NAME      Import only jobs owned by an original legacy client token\n"
                "  mcp                          Serve MCP over stdio; selected instance must be open\n"
                "Offline calls: bookmarks_list, history_list, settings_get, profile_info.\n"
                "Enable external automation in browser Settings; control is separately opt-in.\n"
                "Exit: 0 success, 2 arguments, 3 instance unavailable, 4 denied, 5 failure, 6 cancelled.\n";return 0;
        }
        auto command=positional[0];if(command=="serve")return agent(profile,control,external);
        if(command=="mcp"){if(offline)throw Error("InvalidArguments","MCP requires a running instance");return stdio_proxy(pair(profile,client=="terminal"?"mcp":client));}
        Registry local(Policy::allow_all());register_operations(local,[&](const std::string& n,const Json& a,const CallContext&){return read_profile(profile,n,a);});
        Json connection;if(!offline&&std::filesystem::exists(profile/"sdk/connection.bin"))connection=pair(profile,client,legacy_env);
        if(command=="list"){auto result=connection.is_object()&&!connection.empty()?rpc(connection,{{"method","describe"}}):Json{{"sdkVersion",version},{"scope","offline browser catalogue"},{"operations",local.describe()}};std::cout<<result.dump()<<'\n';return 0;}
        if(command=="describe"){
            if(positional.size()!=2)throw Error("InvalidArguments","describe requires an operation name");auto result=connection.empty()?local.describe(positional[1]):rpc(connection,{{"method","describe"},{"name",positional[1]}});std::cout<<result.dump()<<'\n';return 0;
        }
        auto arguments=parse_json(json);if(!arguments.is_object())throw Error("InvalidArguments","JSON arguments must be an object");std::string operation;
        if(command=="invoke"){if(positional.size()!=2)throw Error("InvalidArguments","invoke requires an operation name");operation=positional[1];}
        else if(command=="jobs"){
            if(positional.size()<2)throw Error("InvalidArguments","jobs requires submit/list/get/cancel/delete");auto action=positional[1];operation="jobs_"+action;
            if(action=="submit"){if(positional.size()!=3)throw Error("InvalidArguments","jobs submit requires an operation name");arguments={{"operation",positional[2]},{"arguments",arguments}};}
            else if(action!="list"){if(positional.size()!=3)throw Error("InvalidArguments","Job ID required");arguments={{"id",positional[2]}};}
        }else throw Error("InvalidArguments","Unknown command");
        Json result;
        if(offline)result=local.invoke(operation,arguments);
        else{if(connection.empty())throw Error("InstanceRequired","Enable automation in the selected running instance, or choose an explicitly supported --offline operation");result=rpc(connection,{{"method","invoke"},{"name",operation},{"arguments",arguments}});}
        std::cout<<result.dump()<<'\n';return 0;
    }catch(const Error& e){std::cerr<<Json{{"error",{{"code",e.code},{"message",redact_error_message(e.what())}}}}.dump()<<'\n';if(e.code=="AccessDenied"||e.code=="AuthenticationRequired")return 4;if(e.code=="InstanceRequired"||e.code=="ConnectionFailed"||e.code=="NetworkError")return 3;if(e.code=="Cancelled")return 6;if(e.code=="InvalidArguments"||e.code=="InvalidParameters"||e.code=="SchemaValidation"||e.code=="UnknownOperation")return 2;return 5;}
    catch(const std::exception& e){std::cerr<<Json{{"error",{{"code","Failure"},{"message",redact_error_message(e.what())}}}}.dump()<<'\n';return 5;}
}
