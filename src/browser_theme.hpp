// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cyui/Theme.h>

namespace cyurium {
cyui::Palette browser_palette(bool dark);
void apply_browser_theme(bool dark,float scale);
void set_browser_logo(ImTextureRef texture);
ImTextureRef browser_logo();
}
