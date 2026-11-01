// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "json.hpp"
#include <vector>
#include <string>

namespace cyurium {
// Privileged engine state shared by all presentation adapters. Commands are
// produced only by native UI; web content never receives this channel.
struct BrowserServices {
    nlohmann::json state=nlohmann::json::object();
    std::vector<nlohmann::json> commands;
    bool hardware_acceleration=true,restore_session=true,recovered_session=false,restore_forms=false;
    bool sdk_enabled=false,sdk_control=false;
    bool assistant_visible=false,assistant_started=false;
    std::vector<nlohmann::json> assistant_commands;
    nlohmann::json assistant=nlohmann::json::object();
    std::string notice;
    std::uint64_t prompt_window=0;
    void request(std::string type,nlohmann::json data=nlohmann::json::object()) {
        data["type"]=std::move(type);commands.push_back(std::move(data));
    }
};
}
