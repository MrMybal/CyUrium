# CyUrium 0.4.0 — Alpha 1

[README](README.md) · [Français](VERSION.fr.md) · [Issue tracker](https://github.com/MrMybal/CyUrium/issues)

## Availability

**0.4.0 Alpha 1** is an experimental Windows x64 browser for testing and feedback.
The [GitHub release](https://github.com/MrMybal/CyUrium/releases/tag/v0.4.0-alpha.1)
provides a per-user installer, portable ZIP, corresponding source archive and SHA-256 checksums.
macOS and Linux retain desktop interface build targets; native Gecko integration and SDK execution remain unvalidated there.
Android/Meta Quest development belongs to CyVR_Urium.

## Changes since 0.3.0

- Native **CyMCPSDK 0.7.0** replaces the CyToolsCore and Python runtime integration.
- `CyUriumCLI.exe` lists operations, describes schemas and invokes real browser commands with JSON input/output, including supported offline profile reads.
- Chat, CLI and external MCP clients share one registry and the browser's read/control permissions. The existing `CyUrium.exe --mcp` entry point is preserved.
- Native conversations support streamed answers, cancellation, explicit page attachments, provider/model discovery and checkpoint restoration. Long histories and messages can be paged without discarding the stored transcript.
- Explicitly selected remote MCP tools and resources can be used in chat. Cache, notifications, compact discovery, chunked results, temporary disk storage, metadata measurements and API context windows have separate settings.
- Durable jobs retain their complete results across restarts and resource expiry. Legacy jobs can be imported using the original owner's credential; interrupted work is not replayed.
- Large UI/companion messages are written progressively through the nonblocking pipe. Concurrent CLI pairing preserves the same client identity.
- The bundled web engine is updated to **Gecko ESR 153.7.0**, using the official Mozilla distribution.

Bookmarks, preferences, draggable tabs, independent windows, permissions, downloads,
extensions and bundled uBlock Origin remain part of the browser.

## Pinned components

| Component | Version | Role |
| --- | --- | --- |
| CyUrium | 0.4.0 · Alpha 1 | Native browser and console companion. |
| Gecko | 153.7.0 ESR | Windows web runtime. |
| Dear ImGui / SDL | 1.92.5-docking / 3.2.28 | Desktop interface and GPU rendering. |
| uBlock Origin | 1.75.0 | Bundled Mozilla-signed extension. |
| CyMCPSDK | 0.7.0, revision `791d4d7d7680213b282ec8529d248a10f8494d77` | Core, Platform, Http, MCP and AI; compiled from source. |

Exact source/archive pins are in `sdk/cymcpsdk.lock.json`, `engine/runtime.lock.json`
and the native dependency configuration. No Python runtime is needed to run the browser.
Python remains a developer build/test dependency.

## Documented limitations

- **Platforms:** Windows x64 is the validated browser distribution. Linux tab positioning requires X11/XWayland; native Wayland positioning is unsupported.
- **Extensions:** some Firefox UI-dependent APIs, sidebars, context menus and tab/window paths need further adaptation; not every Firefox extension is compatible.
- **Web compatibility:** device capture, varied websites, HTML/video fullscreen and accessibility need broader target validation.
- **AI providers:** installed CLI tools/accounts or configured APIs/local services are required. Available tools, streaming and native resumption depend on the selected transport. Real provider accounts were not used for the protocol scenarios; no model or CLI is installed automatically.
- **Sessions:** form restoration is optional and off by default on Windows. Password/file/hidden and recognized payment/one-time-code fields are excluded. DPAPI protects session/conversation records for the account; the entire Gecko profile and preferences are not encrypted by it.
- **Recovery:** interrupted jobs and turns are recorded without replay. Cancelling a request does not undo a browser action already performed. Closely spaced automatic engine restarts are limited to three.
- **Data bounds:** private records are limited to 16 MiB; page snapshots cover the main document and the explicitly requested text limit. SDK references expire according to their configured retention; durable job results remain until deletion.
- **Updates and installer:** alpha updates use manual installer/ZIP downloads. Authenticode and the publisher-signed automatic-update channel remain unconfigured. Uninstallation preserves the profile.
- **Performance:** interface metrics exclude Gecko processes and GPU memory; no cross-platform browsing benchmark is claimed.

## Migration and feedback

Existing bookmarks and settings keep their formats. Legacy conversations are preserved;
if a native CLI session ID was not stored, resumption requires an explicit new session.
Legacy HTTP/Python clients must migrate to the native CLI/MCP interface.
See the [SDK and migration guide](docs/sdk.md).

Report reproducible problems in the [issue tracker](https://github.com/MrMybal/CyUrium/issues)
with platform, steps and expected behavior. The limitations above describe current support;
they are not a promise that other bugs are absent.

[Previous 0.3.0 Alpha 1 release](https://github.com/MrMybal/CyUrium/releases/tag/v0.3.0-alpha.1)
· [Architecture](docs/architecture.md) · [Release preparation](docs/releases.md)
