# Third-party components

CyUrium's original code and documentation are licensed under GPL-3.0-only;
see `LICENSE` and `NOTICE`. The following components retain their own licences.

| Component | Source | License |
| --- | --- | --- |
| Dear ImGui 1.92.5-docking | https://github.com/ocornut/imgui/tree/v1.92.5-docking | MIT |
| SDL 3.2.28 | https://github.com/libsdl-org/SDL/tree/release-3.2.28 | zlib |
| CyUI theme and widgets | Source snapshot in `third_party/cyui/`, with original file hashes in `snapshot.sha256` | MIT, copyright 2026 Cyberalien |
| nlohmann JSON 3.12.0 | https://github.com/nlohmann/json/tree/v3.12.0 | MIT |
| Mozilla Gecko ESR 153.6.0 | Official Firefox ESR Windows x64 distribution; exact archive/source in `engine/runtime.lock.json` | MPL 2.0 and bundled third-party licenses |
| uBlock Origin 1.75.0 | Unchanged Mozilla-signed XPI from Raymond Hill's official AMO listing; pinned in `engine/bundled-extensions.lock.json` | GPL-3.0-only; licence, readable code, filter lists and third-party notices inside the XPI; upstream source archive included in `engine/app/extensions/sources/` |
| Urium emblem | Original owner-supplied CyVR_Urium PNG, unchanged | GPL-3.0-only for the project-owner artwork distributed with CyUrium; see `NOTICE` |
| CyToolsCore 0.10.1 | https://github.com/MrMybal/CyToolsCore | MIT; `LICENCES/CyToolsCore-MIT.txt` |
| CyUEMCPSDK 0.1.8 provider snapshot | https://github.com/MrMybal/CyUEMCPSDK | MIT; `LICENCES/CyUEMCPSDK-MIT.txt` |
| Embedded Python and MCP/SDK dependencies | Pinned by `sdk/runtime.lock.json` and `sdk/requirements.lock.json` | Original notices included in `LICENCES/SDK/` in the package |
| markdown-it-py 4.2.0 / mdurl 0.1.2 | https://github.com/executablebooks/markdown-it-py and https://github.com/executablebooks/mdurl | MIT; build-time showcase HTML export only, not an application or site runtime dependency |
| Roboto Medium | Dear ImGui's `misc/fonts/Roboto-Medium.ttf` | Apache 2.0 |
| Inno Setup 7.1.1 | https://github.com/jrsoftware/issrc/tree/is-7_1_1 | Inno Setup license, preserved in `LICENCES/Inno-Setup.txt`; Windows installer/uninstaller only |

The original licenses are copied beside the executable in `licenses/` and
`assets/Roboto-LICENSE.txt`. CyUI's original license and source fingerprints
are also preserved in `third_party/cyui/`. The SDL and Dear ImGui release
archives are checked against the SHA-256 values in `cmake/Dependencies.cmake`.

The Windows package includes Gecko binaries from the official Mozilla Firefox
ESR archive. The Mozilla launcher runs a separate CyUrium application; the
Firefox frontend is not used. Native runtime binaries are unmodified. The app
archive registers the CyUrium host and its preferences; the host sources and
assembly recipe are included in `engine/host-source/`. Mozilla source version
and archive hash are recorded in `engine/runtime.lock.json`.

`engine/MPL-2.0.txt` and `engine/ThirdPartyLicenses.html` preserve the Mozilla
license and the bundled platform’s notices. No Wolvic or Quest engine binary
is included. The copied Urium logo is documented in `assets/branding/README.md`.
