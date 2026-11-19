// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cymcp/ai.hpp>
#include <cymcp/mcp.hpp>
#include <cymcp/performance.hpp>
#include <cymcp/changes.hpp>
#include <filesystem>

namespace cyurium::automation {
using namespace cymcp;
using Invoke=std::function<Json(const std::string&,const Json&,const CallContext&)>;
const Json& operation_catalog();
bool read_only(const std::string&);
void register_operations(Registry&,Invoke);
std::filesystem::path default_profile();
Json load_private(const std::filesystem::path&,Json fallback=Json::object());
void save_private(const std::filesystem::path&,const Json&);
Json read_profile(const std::filesystem::path&,const std::string&,const Json&);
Json rpc(const Json& connection,const Json& request,const Cancellation& = {});
Json pair(const std::filesystem::path&,const std::string& client_name,const std::string& legacy_token_env={});
Json legacy_jobs(const std::filesystem::path&,const std::string& token);
int agent(const std::filesystem::path&,bool control,bool external);
}
