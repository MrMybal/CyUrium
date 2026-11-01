# Bundled extensions

uBlock Origin 1.75.0, copyright Raymond Hill and contributors, is included as
an **unchanged Mozilla-signed WebExtension**, licensed under GPL-3.0-only.
It remains a separate extension, managed through CyUrium's Extensions page.

Official project: https://github.com/gorhill/uBlock
Official listing: https://addons.mozilla.org/firefox/addon/ublock-origin/

The adjacent `ublock-origin-1.75.0.xpi` is a ZIP archive containing the complete
GPL text (`LICENSE.txt`), the readable extension sources, the exact bundled
filter lists and their notices, and its third-party libraries and licences.
The adjacent `sources/uBlock-1.75.0-source.tar.gz` also supplies the upstream
source tree and build scripts for the same release, including the source of
generated WebAssembly modules. No extension file has been changed. The exact
download URLs and SHA-256 digests are in `bundled-extensions.json`.

Installation occurs once per Gecko profile, before its first web navigation.
Existing installations, including disabled or newer versions, are preserved.
Disabling or uninstalling the extension is remembered across browser restarts.
The normal signed-extension update action remains available.
