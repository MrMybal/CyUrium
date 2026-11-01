// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "browser_workspace.hpp"
namespace cyurium {
class SessionStore {
public:
    explicit SessionStore(std::filesystem::path profile):path_(std::move(profile)/"session.json"){}
    bool restore(BrowserWorkspace&);
    bool checkpoint(const BrowserWorkspace&,bool clean=false);
    bool recovered()const{return recovered_;}
    std::string error;
private:
    std::filesystem::path path_;
    nlohmann::json last_;
    bool recovered_=false;
};
}
