// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_theme.hpp"

namespace cyurium {
namespace { ImTextureRef logo_texture; }
void set_browser_logo(ImTextureRef texture) { logo_texture=texture; }
ImTextureRef browser_logo() { return logo_texture; }
cyui::Palette browser_palette(bool dark) {
    using cyui::rgb;
    cyui::Palette p{};
    p.bg0=rgb(dark?0x191713:0xece7df); // warm tab strip, matching CyVR_Urium
    p.bg1=rgb(dark?0x14130f:0xfffaf2);
    p.bg2=rgb(dark?0x1b1915:0xfffdf9);
    p.bg3=rgb(dark?0x26231d:0xeee8de);
    p.bg4=rgb(dark?0x373025:0xe7ddce);
    p.bg5=rgb(dark?0x211d16:0xf5eee2);
    p.text=rgb(dark?0xeee9df:0x30271b);
    p.textDim=rgb(dark?0xb5aa97:0x7d705e);p.textFaint=p.textDim;
    p.line=rgb(dark?0x40372a:0xe2d7c6);p.lineStrong=p.line;p.grid=p.line;p.gridStrong=p.line;
    p.accent=rgb(dark?0xff9700:0xc66d00);p.accentHover=cyui::brighter(p.accent,.1f);
    p.accentDim=rgb(dark?0x473013:0xffe7c1);p.accentText=rgb(0x21190e);
    p.selection=cyui::alpha(p.accent,.22f);
    p.success=rgb(0x28a36b);p.warning=rgb(0xffbd2e);p.error=rgb(0xdf3b45);p.info=p.accent;
    return p;
}
void apply_browser_theme(bool dark,float scale) {
    static bool initialized=false,previous_dark=false;static float previous_scale=0;
    if(initialized&&previous_dark==dark&&previous_scale==scale)return;
    cyui::Config config;config.themeKey=dark?"mid-grey":"light-grey";
    config.density=cyui::Density::compact;config.dpiScale=scale;
    cyui::apply(config);cyui::setPalette(browser_palette(dark));
    initialized=true;previous_dark=dark;previous_scale=scale;
}
}
