// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "automation.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace cyurium::automation {
Json legacy_jobs(const std::filesystem::path& profile,const std::string& token){
    if(token.size()<16||token.size()>1024)throw Error("AuthenticationRequired","Original legacy client credential required");
#ifdef _WIN32
    auto original=profile/"sdk/private/jobs/state.sqlite3";if(!std::filesystem::is_regular_file(original))throw Error("NoLegacyData","No legacy job database in the selected profile");
    auto parent=std::filesystem::absolute(profile/"sdk/private");private_directory(parent);auto copy=parent/("legacy-read-"+random_token(16));private_directory(copy);
    struct Snapshot {std::filesystem::path path,parent;~Snapshot(){std::error_code ec;auto resolved=std::filesystem::weakly_canonical(path,ec);if(!ec&&resolved.parent_path()==std::filesystem::weakly_canonical(parent,ec))std::filesystem::remove_all(resolved,ec);}} snapshot{copy,parent};
    for(auto suffix:{"","-wal","-shm"}){auto source=std::filesystem::path(original.wstring()+std::filesystem::path(suffix).wstring());if(std::filesystem::exists(source)){if(std::filesystem::is_symlink(source)||std::filesystem::file_size(source)>256*1024*1024)throw Error("StorageError","Legacy database snapshot exceeds its bound");std::filesystem::copy_file(source,copy/("state.sqlite3"+std::string(suffix)));}}
    struct sqlite3;struct sqlite3_stmt;
    auto module=LoadLibraryExW(L"winsqlite3.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)throw Error("UnsupportedMigration","Windows SQLite reader is unavailable");
    struct Module {HMODULE handle;~Module(){FreeLibrary(handle);}} library{module};
    auto open=reinterpret_cast<int(*)(const char*,sqlite3**,int,const char*)>(GetProcAddress(module,"sqlite3_open_v2"));
    auto close=reinterpret_cast<int(*)(sqlite3*)>(GetProcAddress(module,"sqlite3_close"));
    auto prepare=reinterpret_cast<int(*)(sqlite3*,const char*,int,sqlite3_stmt**,const char**)>(GetProcAddress(module,"sqlite3_prepare_v2"));
    auto step=reinterpret_cast<int(*)(sqlite3_stmt*)>(GetProcAddress(module,"sqlite3_step"));
    auto text=reinterpret_cast<const unsigned char*(*)(sqlite3_stmt*,int)>(GetProcAddress(module,"sqlite3_column_text"));
    auto finalize=reinterpret_cast<int(*)(sqlite3_stmt*)>(GetProcAddress(module,"sqlite3_finalize"));
    if(!open||!close||!prepare||!step||!text||!finalize)throw Error("UnsupportedMigration","Windows SQLite API is incomplete");
    sqlite3* db=nullptr;if(open(path_utf8(copy/"state.sqlite3").c_str(),&db,1,nullptr)!=0){if(db)close(db);throw Error("StorageError","Cannot read legacy job snapshot");}
    struct Database {sqlite3* db;decltype(close) finish;~Database(){finish(db);}} database{db,close};
    auto read=[&](const char* sql){sqlite3_stmt* statement=nullptr;if(prepare(db,sql,-1,&statement,nullptr)!=0)throw Error("StorageError","Unsupported legacy records schema");Json records=Json::array();int status;
        try{while((status=step(statement))==100){if(records.size()>=4096)throw Error("QuotaExceeded","Legacy record count exceeds import limit");auto bytes=text(statement,0);if(!bytes)throw Error("StorageError","Legacy record is empty");records.push_back(parse_json(reinterpret_cast<const char*>(bytes),16*1024*1024));}if(status!=101)throw Error("StorageError","Legacy database read failed");}catch(...){finalize(statement);throw;}finalize(statement);return records;};
    auto digest=sha256(token);std::string owner;for(const auto& record:read("SELECT value FROM records WHERE kind='clients'"))if(constant_time_equal(record.value("tokenHash",""),digest))owner=record.at("clientId");
    if(owner.empty())throw Error("AccessDenied","Legacy credential does not own these jobs");Json results=Json::array();for(auto& row:read("SELECT value FROM records WHERE kind='jobs'"))if(row.value("clientId","")==owner)results.push_back(std::move(row));return results;
#else
    (void)profile;throw Error("UnsupportedMigration","Legacy Windows job import is available only on Windows");
#endif
}
}
