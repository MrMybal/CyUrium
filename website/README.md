# CyUrium showcase

An English/French static presentation with local application screenshots,
keyboard-accessible image enlargement, application-view tabs and HTML exports
of the project's maintained guides. English is the default; use `?lang=fr` for
French. Language switching keeps the current chapter and unrelated URL parameters.

## Build

From the CyUrium checkout, with Python 3.11 or newer:

```sh
python -m pip install -r tools/requirements-showcase.txt
python tools/build_showcase.py
```

The generated entry is `dist/website/index.html`. Copy the **entire**
`dist/website/` directory to distribute it. Styles, classic scripts, captures,
guides and notices are included. No Node.js, Git metadata, network request or
GitHub account is needed by the build or the generated site's local content.
The optional GitHub source links are supplemental external destinations.

The generated page uses relative paths and can be opened directly, or served
from any static project subpath. Direct `file://` opening must be checked in the
target browser; this task's in-app browser only permits HTTP/HTTPS navigation.
No browser or operating-system fullscreen API is used for image enlargement.
The viewer initially fits the full frame; **Actual size** enables native-resolution
reading with scrolling inside the image area, while its close control stays reachable.

## Local preview

```sh
python -m http.server 8767 --bind 127.0.0.1 --directory dist
```

Open `http://127.0.0.1:8767/website/index.html` or
`http://127.0.0.1:8767/website/index.html?lang=fr`. Stop the server with Ctrl+C.

## Maintained files

| File | Role |
| --- | --- |
| `website/template.html` | Page structure and useful English content rendered during the build. |
| `website/content.json` | Complete English/French visitor copy and accessible labels. |
| `website/media.json` | Capture catalog, dimensions and localized alt/caption keys. |
| `website/assets/` | Original Urium logo and lossless real application captures. |
| `website/main.js` | Languages, menu, tabs, in-page modal and keyboard interactions. |
| `website/styles.css` | Graphite/amber responsive layout and document presentation. |
| `website/config.json` | Optional verified source repository destination. Set `sourceUrl` to `null` to omit source buttons. |
| `tools/build_showcase.py` | Reproducible build, guide export and packaged link/anchor checks. |

The home, settings and bookmarks captures use the application's built-in
`--capture` command with a disposable demonstration profile. The conversation
capture shows a synthetic local-provider exchange from the native integration
fixture. The screenshots retain the real French application interface and full
frame; the page text is translated separately.

Do not edit `dist/website/` by hand. Source changes to styles, scripts or images
produce new content-hashed filenames and updated HTML references in the same
build. Static hosting should revalidate HTML while allowing long-lived caching
of hashed assets.

## Guides and licenses

The existing English/French README and version notes are exported as static
HTML project guides. Their local links include the original French technical
guides and available license/reference files. These exports are not a CyDoc
reader; CyUrium does not currently have an integrated CyDoc application manual.

The showcase and owner-supplied captures use the application's
[GPL-3.0-only license](../LICENSE), within the scope of [NOTICE](../NOTICE).
The build-time Markdown renderer uses MIT-licensed `markdown-it-py` and `mdurl`;
neither package runs in or is bundled with the generated browser page.

To recheck the generated artifact:

```sh
python tools/build_showcase.py --verify-only
```

Building and previewing create no deployment. No hosting workflow is installed.
Publishing the generated artifact or enabling a host is a separate operation.
