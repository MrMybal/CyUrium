// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <filesystem>
#include <string>
namespace cyurium {
// Returns -1 when this is a normal browser invocation.
int run_update_helper(int argc,char** argv);
bool begin_update(const std::filesystem::path& stage,const std::filesystem::path& install,const std::filesystem::path& profile,std::string& error);
}
