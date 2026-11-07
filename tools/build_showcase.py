# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Build the standalone showcase and HTML exports of the project's real guides.

No Git, network, Node.js, browser service or application runtime is used here.
Install tools/requirements-showcase.txt for the build-time Markdown renderer.
"""
from pathlib import Path
from html import escape, unescape
from html.parser import HTMLParser
import argparse
import hashlib
import json
import os
import re
import shutil
import unicodedata
from urllib.parse import quote, unquote, urlsplit

from markdown_it import MarkdownIt

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'website'


def slug(value):
    value = unicodedata.normalize('NFKD', value).encode('ascii', 'ignore').decode().lower()
    return re.sub(r'[^a-z0-9-]', '', re.sub(r'\s+', '-', value))


class DocumentLinks(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.ids = [], set()

    def handle_starttag(self, tag, attrs):
        for key, value in attrs:
            if key in ('href', 'src') and value:
                self.links.append(value)
            elif key == 'id':
                self.ids.add(value)


def verify(output):
    documents = {}
    for path in output.rglob('*.html'):
        parser = DocumentLinks()
        parser.feed(path.read_text(encoding='utf-8'))
        documents[path.resolve()] = parser
    for path, parser in documents.items():
        for link in parser.links:
            parsed = urlsplit(link)
            if parsed.scheme:
                if parsed.scheme != 'https':
                    raise ValueError('Unsupported link protocol in ' + path.name)
                continue
            if parsed.path.startswith('/'):
                raise ValueError('Absolute site path: ' + link)
            target = (path.parent / unquote(parsed.path)).resolve() if parsed.path else path
            if not target.is_relative_to(output) or not target.is_file():
                raise ValueError('Missing local destination: ' + link + ' in ' + path.name)
            if parsed.fragment and target in documents and unquote(parsed.fragment) not in documents[target].ids:
                raise ValueError('Missing section: ' + link + ' in ' + path.name)
    return len(documents)


def build(output):
    output = output.resolve()
    # Before any recursive removal, validate the final absolute output directory.
    if not any(output.is_relative_to(ROOT / base) and output != ROOT / base for base in ('dist', '.cache')):
        raise ValueError('Choose a generated output below this checkout\'s dist/ or .cache/.')
    if output.exists():
        shutil.rmtree(output)
    (output / 'assets').mkdir(parents=True)
    copy = json.loads((SOURCE / 'content.json').read_text(encoding='utf-8'))
    media = json.loads((SOURCE / 'media.json').read_text(encoding='utf-8'))
    config = json.loads((SOURCE / 'config.json').read_text(encoding='utf-8'))
    if set(copy) != {'en', 'fr'} or set(copy['en']) != set(copy['fr']):
        raise ValueError('English and French translation keys differ')
    for language, strings in copy.items():
        for key, value in strings.items():
            if not isinstance(value, str) or not value.strip():
                raise ValueError('Empty translation: ' + language + '/' + key)
    version = re.search(r'project\(CyUrium VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')).group(1)
    if any(version not in copy[lang]['hero.status'] for lang in copy):
        raise ValueError('Showcase and application versions differ')
    source_url = config.get('sourceUrl')
    if source_url and (urlsplit(source_url).scheme != 'https' or urlsplit(source_url).username):
        raise ValueError('Source URL must use HTTPS without credentials')

    def asset(path):
        data = path.read_bytes()
        name = path.stem + '.' + hashlib.sha256(data).hexdigest()[:12] + path.suffix
        target = output / 'assets' / name
        target.write_bytes(data)
        return './assets/' + name

    assets = {name: asset(SOURCE / file) for name, file in [('styles', 'styles.css'), ('main', 'main.js')]}
    assets['urium-logo'] = asset(SOURCE / 'assets/urium-logo.png')
    for item in media:
        item['src'] = asset(SOURCE / 'assets' / item['file'])
        assets[item['id']] = item['src']
        if any(item[key] not in copy['en'] for key in ('alt', 'caption')):
            raise ValueError('Missing media translation')
    if len({m['id'] for m in media}) != len(media):
        raise ValueError('Duplicate media ID')
    routes = {
        'en': {'guide': './guides/README.html', 'version': './guides/VERSION.html',
               'build': './guides/README.html#source-build-and-required-libraries',
               'conversation': './guides/README.html#about-cyurium'},
        'fr': {'guide': './guides/README.fr.html', 'version': './guides/VERSION.fr.html',
               'build': './guides/README.fr.html#sources-compilation-et-bibliotheques-necessaires',
               'conversation': './guides/README.fr.html#a-propos-de-cyurium'}
    }
    t = lambda key: escape(copy['en'][key], quote=True)

    def text(tag, key, attrs=''):
        return f'<{tag} data-i18n="{key}" {attrs}>{t(key)}</{tag}>'

    def shot(name):
        item = next(m for m in media if m['id'] == name)
        loading = 'fetchpriority="high"' if name == 'home' else 'loading="lazy"'
        return (f'<figure class="screen"><button class="shot" type="button" data-shot="{name}" '
                f'aria-label="{t("viewer.open")}: {t(item["caption"])}">'
                f'<img src="{item["src"]}" width="{item["width"]}" height="{item["height"]}" '
                f'alt="{t(item["alt"])}" data-i18n-alt="{item["alt"]}" {loading} decoding="async"></button>'
                f'<figcaption><span data-i18n="{item["caption"]}">{t(item["caption"])}</span>'
                '<span class="expand-mark" aria-hidden="true">↗</span></figcaption></figure>')

    def replace(match):
        kind, key = match.groups()
        if kind == 't': return t(key)
        if kind == 'asset': return assets[key]
        if kind == 'doc': return routes['en'][key]
        if kind == 'shot': return shot(key)
        if kind == 'source':
            return f'<a class="source-link" href="{escape(source_url)}">GitHub <span aria-hidden="true">↗</span></a>' if source_url else ''
        if kind == 'data':
            return json.dumps({'copy': copy, 'media': media, 'routes': routes}, ensure_ascii=False, separators=(',', ':')).replace('<', '\\u003c')
        if kind == 'workflow':
            return ''.join(f'<article><span class="step-index">0{i}</span>{text("h3",f"browser.step{i}.title")}{text("p",f"browser.step{i}.body")}</article>' for i in range(1, 4))
        if kind == 'essentials':
            return ''.join(f'<article><span class="item-index" aria-hidden="true">0{i}</span><div>{text("h3",f"privacy.{name}.title")}{text("p",f"privacy.{name}.body")}</div></article>' for i, name in enumerate(('ads','permissions','sessions','gpu'),1))
        if kind == 'platforms':
            return ''.join(f'<article class="platform">{text("h3",f"availability.{name}")}{text("p",f"availability.{name}State", "class=platform-state")}{text("p",f"availability.{name}Body")}</article>' for name in ('windows', 'other'))
        if kind == 'faq':
            return ''.join(f'<details><summary data-i18n="faq.q{i}">{t(f"faq.q{i}")}</summary>{text("p",f"faq.a{i}")}</details>' for i in range(1,5))
        raise ValueError('Unknown template token: ' + match.group(0))

    template = (SOURCE / 'template.html').read_text(encoding='utf-8')
    html = re.sub(r'\{\{([a-z]+):([a-zA-Z0-9._-]+)\}\}', replace, template)
    if '{{' in html:
        raise ValueError('Unresolved showcase template token')
    (output / 'index.html').write_text(html, encoding='utf-8', newline='\n')
    (output / 'legal').mkdir()
    for name in ('LICENSE', 'NOTICE'):
        shutil.copy2(ROOT / name, output / 'legal' / (name + '.txt'))

    pending = [ROOT / name for name in ('README.md', 'README.fr.md', 'VERSION.md', 'VERSION.fr.md', 'THIRD_PARTY_NOTICES.md')]
    handled, copied = set(), {}
    md = MarkdownIt('commonmark', {'html': True}).enable('table')

    def heading(tokens, index, options, env):
        token = tokens[index]
        token.attrSet('id', slug(tokens[index + 1].content))
        return md.renderer.renderToken(tokens, index, options, env)

    md.add_render_rule('heading_open', lambda self, tokens, idx, options, env: heading(tokens, idx, options, env))

    def document_path(source):
        return output / 'guides' / source.relative_to(ROOT).with_suffix('.html')

    def relative(destination, current):
        return quote(Path(os.path.relpath(destination, current.parent)).as_posix(), safe='/.-')

    while pending:
        source = pending.pop(0).resolve()
        if source in handled:
            continue
        handled.add(source)
        destination = document_path(source)
        rendered = md.render(source.read_text(encoding='utf-8'))

        def rewrite(match):
            attribute, value = match.groups()
            parsed = urlsplit(unescape(value))
            if parsed.scheme or parsed.netloc or not parsed.path:
                return match.group(0)
            local = (source.parent / unquote(parsed.path)).resolve()
            if not local.is_relative_to(ROOT) or not local.is_file():
                raise ValueError('Missing documentation source: ' + parsed.path + ' in ' + source.name)
            if local.suffix.lower() == '.md':
                target = document_path(local)
                pending.append(local)
            elif local.suffix.lower() in ('.png', '.webp', '.jpg', '.ico'):
                if local not in copied:
                    copied[local] = output / asset(local).removeprefix('./')
                target = copied[local]
            else:
                allowed = ('.txt', '.json', '.sha256', '.mit', '')
                if local.suffix.lower() not in allowed:
                    raise ValueError('Review documentation attachment before bundling: ' + local.name)
                target = output / 'references' / local.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(local, target)
            url = relative(target, destination) + ('#' + parsed.fragment if parsed.fragment else '')
            return attribute + '="' + escape(url, quote=True) + '"'

        rendered = re.sub(r'(href|src)="([^"]+)"', rewrite, rendered)
        language = 'en' if source.name in ('README.md', 'VERSION.md', 'THIRD_PARTY_NOTICES.md') or source.is_relative_to(ROOT / 'third_party') or source.name == 'BUNDLED_EXTENSIONS.md' else 'fr'
        # French technical guides remain explicitly French; their overview links
        # lead to the actual complete English/French README editions.
        c = copy[language]
        guide = relative(document_path(ROOT / ('README.fr.md' if language == 'fr' else 'README.md')), destination)
        notes = relative(document_path(ROOT / ('VERSION.fr.md' if language == 'fr' else 'VERSION.md')), destination)
        stylesheet = relative(output / assets['styles'].removeprefix('./'), destination)
        back = relative(output / 'index.html', destination) + ('?lang=fr' if language == 'fr' else '')
        title = re.search(r'<h[12][^>]*>(.*?)</h[12]>', rendered, re.S)
        title = re.sub('<[^>]+>', '', title.group(1)) if title else source.stem
        if source in (ROOT / 'README.md', ROOT / 'README.fr.md'):
            title = c['docs.guide']
        header = (f'<header class="site-header document-header"><div class="header-inner wrap">'
                  f'<a class="brand" href="{back}">CyUrium</a><nav aria-label="{escape(c["docs.languages"])}">'
                  f'<a href="{back}">{escape(c["docs.back"])}</a><a href="{guide}">{escape(c["docs.guide"])}</a>'
                  f'<a href="{notes}">{escape(c["docs.version"])}</a>'
                  f'<a href="{relative(document_path(ROOT/"README.md"),destination)}" lang="en">English</a>'
                  f'<a href="{relative(document_path(ROOT/"README.fr.md"),destination)}" lang="fr">Français</a></nav></div></header>')
        document = (f'<!doctype html><html lang="{language}" class="document-page"><head><meta charset="utf-8">'
                    f'<meta name="viewport" content="width=device-width, initial-scale=1"><title>{escape(unescape(title))} — CyUrium</title>'
                    f'<link rel="stylesheet" href="{stylesheet}"></head><body>'
                    f'<a class="skip" href="#document">{escape(c["skip"])}</a>{header}'
                    f'<main id="document" class="document-main">{rendered}</main></body></html>')
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(document, encoding='utf-8', newline='\n')
    pages = verify(output)
    print(f'Built {pages} HTML pages with {len(media)} application captures: {output.relative_to(ROOT).as_posix()}/index.html')
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/website')
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    if args.verify_only:
        print('Verified HTML pages:', verify(args.output.resolve()))
    else:
        build(args.output)
