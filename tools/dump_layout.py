#!/usr/bin/env python3
"""Read every control position, label, font and tooltip from the approved mockup page with Playwright and write
docs/mockup/layout.json. tools/gen_layout.py turns it into src/editor/Layout.h, so the native editor takes its
layout from the mockup itself and the two cannot drift apart. Run from the repo root."""
import asyncio, json
from playwright.async_api import async_playwright
src = open('docs/mockup/Apurrr2500.html').read()
open('/tmp/apurr_mock_wrapped.html', 'w').write('<!doctype html><html><head><meta charset="utf-8"></head><body>' + src + '</body></html>')
JS = r"""
() => {
  const stg = document.querySelector('#stage'), sr = stg.getBoundingClientRect();
  const r2 = v => Math.round(v * 100) / 100;
  const R = e => { const r = e.getBoundingClientRect(); return [r2(r.left - sr.left), r2(r.top - sr.top), r2(r.width), r2(r.height)]; };
  const RR = r => [r2(r.left - sr.left), r2(r.top - sr.top), r2(r.width), r2(r.height)];
  const q = s => document.querySelector(s), qa = s => [...document.querySelectorAll(s)];
  const rgb = c => { const m = c.match(/[\d.]+/g).map(Number); return (m[0] << 16) | (m[1] << 8) | m[2]; };
  const TOK = {}; // ink dim title accent in look C
  const cs0 = getComputedStyle(document.querySelector('#app'));
  const hex = n => { const s = cs0.getPropertyValue(n).trim(); return parseInt(s.slice(1), 16); };
  TOK.ink = hex('--ink'); TOK.dim = hex('--dim'); TOK.title = hex('--title'); TOK.accent = hex('--accent');
  const colTok = c => { const v = rgb(c); return v === TOK.ink ? -1 : v === TOK.dim ? -2 : v === TOK.title ? -3 : v === TOK.accent ? -4 : v; };
  const lc = s => { s = (s || '').trim(); return ({ 'var(--g)': 0, 'var(--y)': 1, 'var(--o)': 2, 'var(--r)': 3, 'var(--b)': 4, 'var(--p)': 5 })[s] ?? -1; };
  // text record: tight rect of the text itself, font facts, colour
  const T = (el, node) => {
    const rng = document.createRange(); rng.selectNodeContents(node || el);
    const cs = getComputedStyle(el);
    const rects = rng.getClientRects(); const rect = RR(rng.getBoundingClientRect());
    return { t: (node || el).textContent, r: rect, fs: parseFloat(cs.fontSize), mono: cs.fontFamily.indexOf('Barlow') < 0, fw: parseInt(cs.fontWeight), it: cs.fontStyle === 'italic',
             ls: cs.letterSpacing === 'normal' ? 0 : parseFloat(cs.letterSpacing), up: cs.textTransform === 'uppercase', col: colTok(cs.color) };
  };
  const out = {};
  for (const [k, s] of Object.entries({ cheekL: '#cheekL', cheekR: '#cheekR', main: '.main', hdr: '.hdr', body: '.body', disp: 'section.disp', ctl: '#ctl', meters: '.meters', foot: '.foot', logo: '.brand svg', rig: '.rig' })) out[k] = R(q(s));
  out.nameA = T(q('.brand .name'), q('.brand .name').firstChild);
  out.nameB = T(q('.brand .name b'));
  out.s87 = T(q('.brand .s87'));
  out.hbtn = {};
  for (const id of ['pPrev', 'pName', 'pNext', 'pSave', 'bBypass', 'bGuide', 'bSettings', 'bAbout']) { const e = q('#' + id); out.hbtn[id] = { r: R(e), tip: e.dataset.tip, t: e.textContent, fs: parseFloat(getComputedStyle(e).fontSize), fw: parseInt(getComputedStyle(e).fontWeight) }; }
  out.cv = {};
  for (const id of ['cvCurve', 'cvFilt', 'cvHist', 'cvNeedle', 'cvStep', 'cvIn', 'cvGR', 'cvOut']) { const e = q('#' + id); out.cv[id] = { r: R(e), tip: e.dataset.tip, w: e.width, h: e.height }; }
  out.caps = qa('.dcell .cap').map(e => Object.assign(T(e), { id: e.id || '' }));
  out.sh = qa('.sh').map(e => ({ r: R(e), col: lc('var(--g)'), bcol: getComputedStyle(e).borderBottomColor, texts: [...e.children].map(c => T(c)) }));
  out.kc = qa('.kc').map(c => { const kn = c.querySelector('.knob'), lab = c.querySelector('.lab'), v = c.querySelector('.kval'); return { label: T(lab), aria: kn.getAttribute('aria-label'), knob: R(kn), val: R(v), tip: kn.dataset.tip, kcol: getComputedStyle(c).getPropertyValue('--kc').trim(), cr: R(c) }; });
  out.strips = qa('.strip').map(s => [...s.children].map(b => ({ r: R(b), t: b.textContent, tip: b.dataset.tip, fs: parseFloat(getComputedStyle(b).fontSize) })));
  out.segs = qa('.seg').map(s => ({ label: T(s.querySelector('.sl')), btns: [...s.querySelectorAll('.sbtn')].map(b => ({ r: R(b), t: b.textContent, tip: b.dataset.tip, lc: lc(b.style.getPropertyValue('--lc')), tr: RR(b.querySelector('span').getBoundingClientRect()), dot: R(b.querySelector('i')), fs: parseFloat(getComputedStyle(b).fontSize), fw: parseInt(getComputedStyle(b).fontWeight) })) }));
  out.tgls = qa('.tgl').map(b => ({ r: R(b), t: b.textContent, tip: b.dataset.tip, lc: lc(b.style.getPropertyValue('--lc')), tr: RR(b.querySelector('span').getBoundingClientRect()), dot: R(b.querySelector('i')), fs: parseFloat(getComputedStyle(b).fontSize) }));
  out.led = { r: R(q('.kc i.led')), tip: q('.kc i.led').dataset.tip };
  const pl = q('.plate'); out.plate = { r: R(pl), tip: pl.dataset.tip, texts: [...pl.querySelectorAll('.pt,.ps')].map(e => T(e)), svg: R(pl.querySelector('svg')) };
  out.mcap = qa('.meters .cap span').map(e => T(e));
  out.pk = qa('.meters .pk span').map(e => Object.assign(T(e), { id: e.id }));
  out.footL = T(q('.foot span:first-child'));
  out.winsz = T(q('#winsz'));
  out.zoom = R(q('.zoomrow'));
  return out;
}
"""
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(); pg = await b.new_page(viewport={'width': 1400, 'height': 900})
        await pg.goto('file:///tmp/apurr_mock_wrapped.html'); await pg.wait_for_timeout(1500)
        d = await pg.evaluate(JS)
        json.dump(d, open('docs/mockup/layout.json', 'w'), indent=1)
        await pg.screenshot(path='/tmp/apurr_mockup_reference.png', clip={'x': 0, 'y': 0, 'width': 1400, 'height': 720})
        print('knobs', [k['aria'] for k in d['kc']], 'strips', [len(s) for s in d['strips']], 'segs', [len(s['btns']) for s in d['segs']], 'tgls', len(d['tgls']), 'sh', len(d['sh']))
        await b.close()
asyncio.run(main())
