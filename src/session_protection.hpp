// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>
#include <string_view>
namespace cyurium {
std::string protect_session(std::string_view plain);
std::string unprotect_session(std::string_view bytes,bool& legacy);
}
