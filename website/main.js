// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
(() => {
  'use strict';
  const data = JSON.parse(document.getElementById('site-data').textContent);
  const initial = new URL(location.href);
  let language = initial.searchParams.get('lang') === 'fr' ? 'fr' : 'en';
  const media = data.media;
  const menu = document.querySelector('.menu-button');
  const nav = document.getElementById('site-navigation');
  const dialog = document.querySelector('.image-viewer');
  const picture = document.getElementById('viewer-image');
  const stage = document.querySelector('.viewer-stage');
  const zoom = document.querySelector('.viewer-zoom');
  let openedBy = null;
  let current = 0;
  let savedScroll = { x: 0, y: 0 };
  let previousOverflow = '';
  let previousPadding = '';
  const t = key => data.copy[language][key];

  function updateURL(url) {
    try { history.replaceState(null, '', url); }
    catch { location.href = url.href; }
  }
  if (initial.searchParams.has('lang') && !['en', 'fr'].includes(initial.searchParams.get('lang'))) {
    initial.searchParams.set('lang', 'en');
    updateURL(initial);
  }
  function closeMenu(restoreFocus = false) {
    const wasOpen = menu.getAttribute('aria-expanded') === 'true';
    menu.setAttribute('aria-expanded', 'false');
    menu.setAttribute('aria-label', t('nav.menu'));
    nav.dataset.open = 'false';
    if (restoreFocus && wasOpen) menu.focus();
  }
  function updateViewer() {
    const item = media[current];
    picture.src = item.src;
    picture.alt = t(item.alt);
    document.getElementById('viewer-caption').textContent = t(item.caption);
    document.getElementById('viewer-position').textContent = t('viewer.position').replace('{current}', current + 1).replace('{total}', media.length);
    setZoom(false);
  }
  function setZoom(enabled) {
    stage.dataset.zoomed = String(enabled);
    stage.scrollTop = stage.scrollLeft = 0;
    zoom.setAttribute('aria-pressed', String(enabled));
    zoom.textContent = t(enabled ? 'viewer.fit' : 'viewer.zoom');
  }
  zoom.addEventListener('click', () => setZoom(stage.dataset.zoomed !== 'true'));
  function localize() {
    document.documentElement.lang = language;
    document.title = t('meta.title');
    document.querySelector('meta[name="description"]').content = t('meta.description');
    document.querySelector('meta[property="og:title"]').content = t('meta.title');
    document.querySelector('meta[property="og:description"]').content = t('meta.description');
    document.querySelectorAll('[data-i18n]').forEach(el => { el.textContent = t(el.dataset.i18n); });
    document.querySelectorAll('[data-i18n-alt]').forEach(el => { el.alt = t(el.dataset.i18nAlt); });
    document.querySelectorAll('[data-i18n-label]').forEach(el => { el.setAttribute('aria-label', t(el.dataset.i18nLabel)); });
    document.querySelectorAll('[data-lang]').forEach(el => { el.setAttribute('aria-pressed', String(el.dataset.lang === language)); });
    document.querySelectorAll('[data-doc]').forEach(el => { el.href = data.routes[language][el.dataset.doc]; });
    document.querySelectorAll('[data-shot]').forEach(el => {
      const item = media.find(m => m.id === el.dataset.shot);
      el.setAttribute('aria-label', `${t('viewer.open')}: ${t(item.caption)}`);
    });
    menu.setAttribute('aria-label', t(menu.getAttribute('aria-expanded') === 'true' ? 'nav.closeMenu' : 'nav.menu'));
    if (dialog.open) updateViewer();
    else zoom.textContent = t('viewer.zoom');
  }
  document.querySelectorAll('[data-lang]').forEach(button => button.addEventListener('click', () => {
    language = button.dataset.lang;
    const url = new URL(location.href);
    url.searchParams.set('lang', language);
    updateURL(url);
    localize();
  }));
  window.addEventListener('popstate', () => {
    language = new URL(location.href).searchParams.get('lang') === 'fr' ? 'fr' : 'en';
    localize();
  });
  menu.addEventListener('click', () => {
    const expanded = menu.getAttribute('aria-expanded') !== 'true';
    menu.setAttribute('aria-expanded', String(expanded));
    menu.setAttribute('aria-label', t(expanded ? 'nav.closeMenu' : 'nav.menu'));
    nav.dataset.open = String(expanded);
  });
  nav.querySelectorAll('a').forEach(link => link.addEventListener('click', () => { closeMenu(); }));
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && !dialog.open) closeMenu(true);
  });

  const tabs = [...document.querySelectorAll('[data-tool]')];
  function selectTool(button, focus = false) {
    tabs.forEach(tab => {
      const selected = tab === button;
      tab.setAttribute('aria-selected', String(selected));
      tab.tabIndex = selected ? 0 : -1;
      document.getElementById(tab.getAttribute('aria-controls')).hidden = !selected;
    });
    if (focus) button.focus({ preventScroll: true });
  }
  tabs.forEach(tab => tab.addEventListener('click', () => selectTool(tab)));
  document.querySelector('[role="tablist"]').addEventListener('keydown', event => {
    if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
    event.preventDefault();
    let index = tabs.indexOf(document.activeElement);
    if (event.key === 'Home') index = 0;
    else if (event.key === 'End') index = tabs.length - 1;
    else index = (index + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length;
    selectTool(tabs[index], true);
  });

  document.querySelectorAll('[data-shot]').forEach(button => button.addEventListener('click', () => {
    openedBy = button;
    current = media.findIndex(m => m.id === button.dataset.shot);
    savedScroll = { x: window.scrollX, y: window.scrollY };
    previousOverflow = document.body.style.overflow;
    previousPadding = document.body.style.paddingRight;
    updateViewer();
    const scrollbar = window.innerWidth - document.documentElement.clientWidth;
    if (scrollbar > 0) document.body.style.paddingRight = `${parseFloat(getComputedStyle(document.body).paddingRight) + scrollbar}px`;
    document.body.style.overflow = 'hidden';
    window.scrollTo({ left: savedScroll.x, top: savedScroll.y, behavior: 'instant' });
    dialog.showModal();
    dialog.querySelector('.viewer-close').focus({ preventScroll: true });
  }));
  dialog.querySelector('.viewer-close').addEventListener('click', () => dialog.close());
  function changeImage(direction) {
    current = (current + direction + media.length) % media.length;
    updateViewer();
  }
  dialog.querySelector('[data-previous]').addEventListener('click', () => changeImage(-1));
  dialog.querySelector('[data-next]').addEventListener('click', () => changeImage(1));
  dialog.addEventListener('keydown', event => {
    if (event.key === 'Tab') {
      const controls = [...dialog.querySelectorAll('button')];
      const index = controls.indexOf(document.activeElement);
      event.preventDefault();
      controls[(index + (event.shiftKey ? -1 : 1) + controls.length) % controls.length].focus();
    } else if (['ArrowLeft', 'ArrowRight'].includes(event.key)) {
      event.preventDefault();
      changeImage(event.key === 'ArrowRight' ? 1 : -1);
    }
  });
  dialog.addEventListener('click', event => {
    if (event.target !== dialog) return;
    const r = dialog.getBoundingClientRect();
    if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) dialog.close();
  });
  dialog.addEventListener('close', () => {
    document.body.style.overflow = previousOverflow;
    document.body.style.paddingRight = previousPadding;
    picture.removeAttribute('src');
    openedBy?.focus({ preventScroll: true });
    window.scrollTo({ left: savedScroll.x, top: savedScroll.y, behavior: 'instant' });
  });
  localize();
})();
