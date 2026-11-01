// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "session_protection.hpp"
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <dpapi.h>
#endif
namespace cyurium {
namespace {constexpr std::string_view prefix="CYURIUM-DPAPI-1\n";}
std::string protect_session(std::string_view plain){
    if(plain.size()>16*1024*1024)throw std::runtime_error("Session trop grande.");
#ifdef _WIN32
    DATA_BLOB input{DWORD(plain.size()),reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))},output{};
    if(!CryptProtectData(&input,L"CyUrium session",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))throw std::runtime_error("Impossible de protéger la session avec le compte Windows ("+std::to_string(GetLastError())+").");
    std::string bytes(prefix);bytes.append(reinterpret_cast<char*>(output.pbData),output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return bytes;
#else
    return std::string(plain); // The caller strips all form values on these platforms.
#endif
}
std::string unprotect_session(std::string_view bytes,bool& legacy){
    if(bytes.size()>32*1024*1024)throw std::runtime_error("Session trop grande.");
    legacy=!bytes.starts_with(prefix);if(legacy)return std::string(bytes);
#ifdef _WIN32
    bytes.remove_prefix(prefix.size());DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()))},output{};
    if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))throw std::runtime_error("Session chiffrée illisible pour ce compte Windows.");
    std::string plain(reinterpret_cast<char*>(output.pbData),output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);
    if(plain.size()>16*1024*1024)throw std::runtime_error("Session trop grande.");return plain;
#else
    throw std::runtime_error("Cette session est protégée par un compte Windows.");
#endif
}
}
