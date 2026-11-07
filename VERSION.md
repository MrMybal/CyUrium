# CyUrium 0.3.0 — Alpha

[README](README.md) · [Français](VERSION.fr.md) · [Issue tracker](https://github.com/MrMybal/CyUrium/issues)

## Availability

These notes describe the **0.3.0 source version** and the Windows implementation
in this checkout. There is no published application package in
[GitHub Releases](https://github.com/MrMybal/CyUrium/releases) yet.
This alpha is experimental and intended for testing and feedback.

Windows x64 hosts Gecko and the private conversation/SDK runtime. macOS and Linux
currently have desktop interface build targets; their native web integration is
pending. Android/Meta Quest development belongs to the separate CyVR_Urium project.

## Included functionality

- Native Chrome-inspired interface, light/dark themes, scaling, bookmarks and typed-address history.
- Independent web tabs with drag/reorder, native window detachment/attachment,
  and preservation of the document, form state and navigation history during moves.
- Gecko HTML/JavaScript browsing, downloads, signed extensions and bundled uBlock Origin.
- Per-origin permissions, session snapshots, opt-in form restoration and engine recovery.
- Native conversation sidebar, existing provider connectors and separately enabled MCP access through CyToolsCore.
- SDL_GPU interface rendering and Windows Gecko/WebRender hardware acceleration.
- Signed GitHub update implementation with per-file verification and startup rollback;
  the publisher key and dedicated Releases destination remain to be configured.

## Pinned components

| Component | Version | Role |
| --- | --- | --- |
| CyUrium | 0.3.0 · Alpha | Application source version. |
| Gecko | 153.6.0 ESR | Windows web runtime. |
| Dear ImGui / SDL | 1.92.5-docking / 3.2.28 | Native desktop interface. |
| uBlock Origin | 1.75.0 | Bundled Mozilla-signed extension. |
| CyToolsCore / CyUEMCPSDK | 0.10.1 / 0.1.8 | SDK runtime and provider snapshot. |
| CPython / MCP Python SDK | 3.14.10 / 1.30.0 | Private Windows SDK dependencies. |

Versions come from CMake, the engine lockfiles and SDK lockfiles. They describe
this source tree; no binary release has shipped yet.

## Documented limitations

- **Platforms:** Gecko, the packaged SDK companion and native updates are currently
  Windows-only. macOS/Linux runtime behavior has not been validated locally.
  Linux multi-window positioning requires X11/XWayland; native Wayland is unsupported.
- **Extensions:** Firefox UI-dependent APIs need further adaptation, including
  some tab/window creation paths, sidebars and context menus. Not every Firefox extension is compatible.
- **Web compatibility:** HTML/video fullscreen, accessibility, varied sites and
  capture devices still need broader validation. Permission handlers do not establish
  complete device or site compatibility.
- **Session privacy:** form restoration is off by default and available on Windows.
  Password, file, hidden, recognized payment-card and one-time-code fields are excluded.
  DPAPI protects session snapshots and their backup for the Windows account;
  settings, typed-address history and the entire Gecko profile are not encrypted by it.
- **Recovery:** at most three closely spaced automatic engine restarts are attempted;
  manual restart remains available. Restoring a session can restart page activity.
- **AI connectors:** installed CLI tools/accounts or a configured API/local provider
  are required. Codex App Server browser tools use an experimental dynamic-tool
  protocol and need a compatible installed version. Other connectors support chat;
  active-page content must be attached explicitly.
- **Updates:** no download is currently offered. The public Releases destination and
  real publisher key are unset; key rotation and prerelease version suffixes are not implemented.
- **Performance:** interface-only metrics exclude Gecko processes and GPU memory.
  No cross-platform browsing benchmark is claimed.

## Bugs and feedback

No separate bug-by-bug release list is published for 0.3.0. The limitations above
identify incomplete support and remaining validation. Report reproducible problems
in the [issue tracker](https://github.com/MrMybal/CyUrium/issues), including the
platform, steps and expected result. Remove personal page content from attachments.

See the [architecture](docs/architecture.md), [SDK guide](docs/sdk.md) and
[release guide](docs/releases.md), currently in French, for technical details.
