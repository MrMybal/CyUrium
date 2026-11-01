// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "json.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace cyurium {
std::string bridge_random(std::size_t bytes);
std::string bridge_hmac(std::string_view secret,std::string_view text);
std::string bridge_transcript(std::string_view challenge,const nlohmann::json& hello);
// The secret is process-local. It never appears in titles, command lines,
// files or bridge frames. Each connection proves possession in both directions.
class LocalBridge {
public:
    explicit LocalBridge(std::function<void(nlohmann::json)> receive);
    ~LocalBridge();
    std::uint16_t port()const;
    const std::string& secret()const;
    void start(std::uint32_t process_id);
    void send(nlohmann::json message);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
