<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://github.com/MrMybal/CyUrium">Repository</a><br>
      <a href="website/README.md">Showcase · build &amp; preview</a><br>
      <a href="docs/architecture.md">Architecture</a><br>
      <a href="docs/sdk.md">SDK / MCP guide (French)</a><br><br>
      <sub>README</sub><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="assets/branding/urium-logo.png" alt="CyUrium logo" width="128">
    </td>
    <td width="300" align="right" valign="top">
      <p><sub>Application license</sub><br><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p><sub>Version</sub><br><a href="VERSION.md"><strong>0.3.0</strong></a><br><strong>ALPHA</strong></p>
    </td>
  </tr>
</table>

<h1 align="center">CyUrium</h1>

A native desktop browser with a Chrome-inspired interface, Gecko page rendering,
draggable tabs and an integrated AI conversation panel. Its C++20 interface uses
Dear ImGui and SDL3 GPU, with CyVR_Urium's original logo and brown and amber theme.

> **ALPHA · 0.3.0**
>
> An experimental Windows browser for testing and feedback. Some features remain
> incomplete. macOS and Linux currently provide the desktop interface only.
> Read the [version notes and known limitations](VERSION.md).

[![CyUrium's dark interface, with its amber logo, address bar, bookmarks and new-tab search](docs/screenshots/dark.png)](docs/screenshots/dark.png)

Explore the [English/French showcase](website/README.md) for application captures,
an interactive gallery and local HTML guides. Follow its build and preview
instructions to open it locally or copy the complete generated site.

## About CyUrium

Keep pages in independent tabs, drag them along a tab bar, drop them into another
window, or release them outside the bars to create a new window. The document,
form state and navigation history stay with the tab. Escape cancels a drag;
closing a window's last tab closes that window. Bookmarks and preferences are
shared across windows.

Settings, bookmarks, history, downloads and extensions each open in a new tab.
The original page keeps its own tab and playback state. Switch between light and
dark themes and adjust interface scaling from 85% to 150% in Settings.

The **Conversation** button opens a resizable native sidebar with separate drafts,
transcripts and provider sessions. Choose Codex App Server / CLI JSONL, Claude CLI,
Antigravity, OpenCode, Ollama, Claude API or OpenRouter. CLI providers use their
own authentication; API configuration asks for an environment variable name.
Page attachments are explicit, browser tools are read-only by default, and page
control and external MCP access require separate activation. See the
[SDK / MCP guide](docs/sdk.md), currently in French.

<details>
<summary>Keyboard shortcuts</summary>

Use Cmd instead of Ctrl on macOS.

| Shortcut | Action |
| --- | --- |
| Ctrl+T / Ctrl+W | Open / close a tab |
| Ctrl+N / Ctrl+Shift+W | Open / close a window |
| Ctrl+Q | Quit all windows |
| Ctrl+L or F6 | Focus the address bar |
| Ctrl+Tab / Ctrl+Shift+Tab | Switch tabs |
| Ctrl+1–9 | Select a tab |
| Alt+Left / Alt+Right | Back / forward |
| Ctrl+D / Ctrl+Shift+B | Toggle a bookmark / the bookmarks bar |
| Ctrl+H / Ctrl+J | Typed-address history / downloads |
| Ctrl+, | Settings |
| Ctrl++ / Ctrl+- / Ctrl+0 | Interface scale |
| F11 | Fullscreen |

</details>

## Features

- **Gecko browsing:** HTML/JavaScript, network navigation, back/forward history,
  separate content processes, TLS status and certificate details.
- **Downloads and extensions:** persistent download progress, pause/resume,
  cancellation and retry; Mozilla-signed `.xpi` installation with permission
  confirmation, enable/disable, removal, updates, options and HTML popups.
- **Built-in ad blocking:** Mozilla-signed **uBlock Origin 1.75.0** is bundled.
  Its first installation needs no download and navigation waits for filter
  initialization. Existing installations and later disable/removal choices are preserved.
- **Site permissions:** temporary or saved per-origin decisions, editable rules,
  and handlers for location, notifications, camera/microphone, screen capture,
  clipboard, storage, MIDI, XR and local network requests. Unknown requests are denied.
- **Sessions and recovery:** restore windows, selected tabs and history; restart
  Gecko after a crash. Form restoration is optional and off by default on Windows.
  Session snapshots and backups use Windows DPAPI for the current account.
- **GPU rendering:** SDL_GPU for the interface and Gecko/WebRender for Windows
  web content, with a persistent acceleration setting and driver compatibility checks.

---

## Installation

**No packaged application download is published yet.** Build from source using
the requirements below. The current platform status is:

| Platform | Current availability |
| --- | --- |
| Windows x64 | Browser with Gecko, native chat/SDK companion and updater implementation. |
| macOS | Desktop interface build target; native Gecko/SDK integration pending. |
| Linux | Desktop interface build target; native Gecko/SDK integration pending. X11/XWayland required for multi-window positioning. |

1. Obtain the source and install the Windows build tools listed below.
2. Run **Start-CyUrium.cmd** from the checkout. It builds the application when missing,
   or opens the existing package in `dist/windows/CyUrium/`.
3. Keep the complete package together, including `engine/`, `sdk/`, assets and notices.

The Windows package bundles Gecko and its private Python runtime: running it
requires neither an installed Firefox nor system Python. A compatible GPU driver
is required. Android and Meta Quest remain in the separate
CyVR_Urium project.

## Source, build and required libraries

<details>
<summary>Build requirements and commands</summary>

Use **Python 3**, **CMake 3.24+** and a C++20 toolchain. On Windows, install
**Visual Studio 2022 C++ tools**, the **Windows SDK**, **CMake/Ninja** and **7-Zip**.
macOS requires Xcode command-line tools; Linux requires X11/Wayland development libraries.

```sh
git clone https://github.com/MrMybal/CyUrium.git
cd CyUrium
python tools/build.py
python tools/build.py --run
```

Use `python tools/build.py --configuration Debug` for a debug build, or
`python tools/build.py --core-only` for the model and core tests. The Windows C++
runtime is linked statically. Build scripts fetch official archives, verify pinned
hashes and prepare the required runtime and license files.

| Platform | Output | Preferred interface renderer |
| --- | --- | --- |
| Windows x64 | `dist/windows/CyUrium/` | Direct3D 12 |
| macOS | `dist/macos/CyUrium.app` | Metal |
| Linux | `dist/linux/CyUrium/` | Vulkan |

macOS and Linux have CMake targets and a CI matrix, but their runtime behavior
has not been validated locally. Native Wayland lacks the global pointer coordinates
needed by tab dragging; X11 is preferred when available unless an SDL driver is selected.

Debian/Ubuntu dependencies:

```sh
sudo apt install build-essential cmake ninja-build pkg-config libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libegl1-mesa-dev libdrm-dev libgbm-dev libdecor-0-dev
```

The Windows engine comes from the pinned official Firefox ESR distribution.
Mozilla's launcher starts CyUrium's own XUL application and profile; the visible
interface belongs to CyUrium. Moving a web tab reparents its native surface,
preserving the document without copying web rendering through CPU memory.

The build runs native tests. Additional checks include `python tools/test_web.py`,
`python tools/test_services.py`, `python tools/test_adblock.py`,
`python tools/test_sdk.py` and `python tools/test_assistant.py`.
Real-engine scenarios need a desktop, GPU and network connection.
`--metrics` measures the interface process only, excluding Gecko and GPU memory.

Update signing and the dedicated public Releases destination remain unconfigured.
See the [release guide](docs/releases.md), currently in French, before preparing
signed packages or distributing corresponding source.

</details>

### Local showcase

Build the complete English/French showcase, with local captures and HTML project guides:

```sh
python -m pip install -r tools/requirements-showcase.txt
python tools/build_showcase.py
python -m http.server 8767 --bind 127.0.0.1 --directory dist
```

Open `http://127.0.0.1:8767/website/index.html`. See the
[showcase build and preview guide](website/README.md). The generated site can
also be copied as a complete folder; no hosting service is required.


---

## License

CyUrium's original code, documentation and project-owner artwork use
[GNU GPL version 3 only](LICENSE), with the scope described in [NOTICE](NOTICE).
Third-party components retain their own terms.

### Third-party libraries

| Library · source | License · terms | Use / scope |
| --- | --- | --- |
| [Dear ImGui 1.92.5-docking](https://github.com/ocornut/imgui/tree/v1.92.5-docking) | [MIT](https://github.com/ocornut/imgui/blob/v1.92.5-docking/LICENSE.txt) | Native interface, input and multi-window rendering. |
| [SDL 3.2.28](https://github.com/libsdl-org/SDL/tree/release-3.2.28) | [zlib](https://github.com/libsdl-org/SDL/blob/release-3.2.28/LICENSE.txt) | Windowing, events and GPU abstraction. |
| [CyUI snapshot](third_party/cyui/README.md) | [MIT](third_party/cyui/LICENSE) | Bundled theme, DPI and settings widgets; not the whole upstream application library. |
| [nlohmann JSON 3.12.0](https://github.com/nlohmann/json/tree/v3.12.0) | [MIT](third_party/json/LICENSE.MIT) | Native JSON data and protocol handling. |
| [Gecko ESR 153.6.0](engine/runtime.lock.json) | [MPL-2.0](engine/MPL-2.0.txt) and bundled component notices | Windows web engine, distributed with Mozilla's third-party notices. |
| [uBlock Origin 1.75.0](https://github.com/gorhill/uBlock/tree/1.75.0) | [GPL-3.0-only and component notices](engine/BUNDLED_EXTENSIONS.md) | Separate bundled, Mozilla-signed extension with readable sources and filter-list notices. |
| [CyToolsCore 0.10.1](https://github.com/MrMybal/CyToolsCore) | [MIT](LICENCES/CyToolsCore-MIT.txt) | Windows SDK operation runtime and jobs. |
| [CyUEMCPSDK 0.1.8](https://github.com/MrMybal/CyUEMCPSDK) | [MIT](LICENCES/CyUEMCPSDK-MIT.txt) | Portable provider/conversation source snapshot. |
| [CPython 3.14.10](https://www.python.org/downloads/release/python-31410/) | [PSF and component notices](https://docs.python.org/3/license.html) | Private Windows SDK runtime; full text included in `LICENCES/SDK/Python.txt` in the package. |
| [MCP Python SDK 1.30.0](https://github.com/modelcontextprotocol/python-sdk) | [MIT](https://github.com/modelcontextprotocol/python-sdk/blob/main/LICENSE) | External MCP transport; enabled separately from chat. |
| [cryptography](https://github.com/pyca/cryptography) | [Apache-2.0 OR BSD-3-Clause](https://github.com/pyca/cryptography/blob/main/LICENSE) and bundled notices | Python SDK dependency and maintainer-only package signing tool. |
| [markdown-it-py 4.2.0](https://github.com/executablebooks/markdown-it-py) / [mdurl 0.1.2](https://github.com/executablebooks/mdurl) | [MIT](https://github.com/executablebooks/markdown-it-py/blob/master/LICENSE) / [MIT](https://github.com/executablebooks/mdurl/blob/master/LICENSE) | Build-time showcase guide export only; neither runs in the static site or browser application. |
| [Roboto Medium](https://github.com/ocornut/imgui/tree/v1.92.5-docking/misc/fonts) | [Apache-2.0](assets/Roboto-LICENSE.txt) | Bundled interface font. |

This table indexes the main components. Keep the complete
[third-party notices](THIRD_PARTY_NOTICES.md), `licenses/`, `LICENCES/SDK/`,
Gecko notices and extension source/notices supplied with the package.
The [SDK lockfile](sdk/requirements.lock.json) records additional Python dependencies.

## References and links

- [Source repository](https://github.com/MrMybal/CyUrium) · [Issue tracker](https://github.com/MrMybal/CyUrium/issues).
- [Version notes](VERSION.md) · [Releases](https://github.com/MrMybal/CyUrium/releases).
- [Showcase: build and local preview](website/README.md).
- [Architecture](docs/architecture.md), [SDK / MCP](docs/sdk.md) and [release preparation](docs/releases.md) — technical guides currently in French.
