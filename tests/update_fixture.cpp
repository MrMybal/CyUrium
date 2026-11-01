// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
int wmain(int argc,wchar_t** argv){
    wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);auto folder=std::filesystem::path(module).parent_path();
    if(std::filesystem::exists(folder/L"force-fail"))return 1;
    std::filesystem::path profile,health;
    for(int i=1;i+1<argc;++i){if(std::wstring_view(argv[i])==L"--profile")profile=argv[++i];else if(std::wstring_view(argv[i])==L"--update-health")health=argv[++i];}
    if(!health.empty())std::ofstream(health)<<"ok";
    else if(!profile.empty())std::ofstream(profile/L"old-restarted.txt")<<"ok";
    return 0;
}
