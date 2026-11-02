#!/usr/bin/env python3
"""Build docs/manual.html (and docs/manual/*.png) from the real window screenshot docs/screenshot.png and the
control data in docs/mockup/layout.json. CI turns manual.html into a PDF with headless Chromium. Needs Pillow.
usage: python3 tools/gen_manual.py [version]"""
import json, html, sys
from PIL import Image, ImageDraw, ImageFont
ver = sys.argv[1] if len(sys.argv) > 1 else '0.1.0'
L = json.load(open('docs/mockup/layout.json'))
shot = Image.open('docs/screenshot.png').convert('RGB')
S = shot.width / 1360.0
kc = {k['aria']: k for k in L['kc']}
sg = {s['label']['t']: s for s in L['segs']}
tg = {t['t']: t for t in L['tgls']}
hb = L['hbtn']; cv = L['cv']

def joined(btns): return ' '.join('%s: %s' % (b['t'], b['tip']) for b in btns)
# (kind, title, anchor (x,y) in window units, range, default, text)
C = []
def add(title, anchor, rng, dflt, text): C.append((title, anchor, rng, dflt, text))
add('Preset bar', (hb['pName']['r'][0] + 6, hb['pName']['r'][1] + 2), 'arrows, name, Save', 'Start Here', 'Arrows step through presets. The name opens the preset browser (search, type chips, load, export, rename, delete). Double-click the name to rename your own preset. Save stores the settings as your own preset and asks before overwriting.')
for n in ['bBypass', 'bGuide', 'bSettings', 'bAbout']:
    h = hb[n]; add(h['t'], (h['r'][0] + 4, h['r'][1] + 2), 'on / off' if n == 'bBypass' else '', 'off' if n == 'bBypass' else '', h['tip'])
def knob(n, rng, d, extra=''):
    k = kc[n]; add(n, (k['label']['r'][0] - 10, k['label']['r'][1] + 8), rng, d, k['tip'] + extra)
def seg(n, rng, d):
    s = sg[n]; add(n, (s['label']['r'][0] - 10, s['label']['r'][1] + 7), rng, d, joined(s['btns']))
knob('Threshold', '+10 to -20 dBu (0 dBu = -22 dBFS)', '+4 dBu', ' The lamp beside the knob lights while the sound is over the threshold.')
knob('Ratio', '1.5, 2, 3, 4, 6, 10, Inf : 1', '2:1', ' The row of small buttons under the knobs jumps straight to a step.')
knob('Make-up', '0 to +24 dB (Manual)', 'Auto')
seg('Knee', 'Soft 12 dB, Med 6 dB, Hard 0.5 dB', 'Med')
seg('Make-up', 'Auto, Manual', 'Auto')
seg('Tilt filter', 'Norm, Med, Loud', 'Norm')
seg('Type', 'Old, New', 'New')
t = tg['Ext SC']; add('Ext SC', (t['r'][0] + 2, t['r'][1] + 2), 'on / off', 'off', t['tip'])
knob('Link', 'IND, 50 to 100 %', '100 %', ' The row of small buttons under it jumps to a step.')
seg('Link shape', 'Fast, Slow, Both', 'Fast')
knob('Attack', '0.03 to 30 ms, 7 steps', '10 ms')
knob('Release', '50 ms to 2 s, or Var', '200 ms')
knob('Var', '50 ms to 3 s (log)', '150 ms')
seg('Blend', '100 %, X-fade, Par', '100 %')
knob('Mix', '0 to 100 %', '100 %')
seg('Meter shows', 'GR, Out, In', 'GR')
for n in ['IN', 'Level Match']:
    t = tg[n]; add(n, (t['r'][0] + 2, t['r'][1] + 2), 'on / off', 'on' if n == 'IN' else 'off', t['tip'])
for n, nm in [('cvIn', 'In meter'), ('cvGR', 'GR meter'), ('cvOut', 'Out meter')]:
    r = cv[n]['r']; add(nm, (r[0] + 19, r[1] + 372), '-60 to +6 dB' if n != 'cvGR' else '0 to 20 dB', '', cv[n]['tip'])
D = [('Transfer curve', 'cvCurve', 'Input level across (-60 to 0 dB), output level up (-60 to +12 dB). White dashed line is 1:1, the mint line is your compression curve, the purple band is the knee, the pink dashed line is the threshold. The white dot is the live signal.'),
     ('Detector filter', 'cvFilt', 'How the detector hears each frequency, 20 Hz to 20 kHz, -20 to +20 dB. Flat for Norm; lows count less and highs more for Med and Loud.'),
     ('Gain reduction history', 'cvHist', 'The last six seconds of gain reduction, 0 to 20 dB, hanging from the top. Mint filled is left, pink dashed is right.'),
     ('Needle meter', 'cvNeedle', 'Two needles, mint left and pink right. GR mode reads 0 to 20 dB with 0 at the right. Out and In modes read VU, -20 to +3, with 0 VU at -18 dBFS by default (Settings can change it).'),
     ('Old vs New', 'cvStep', 'A built-in test: 1 kHz at -34 dB, a loud burst from 100 to 300 ms, then quiet again, run through your settings by the real compressor code. Mint is New, yellow is Old. It shows the gain reduction over half a second, in dB.')]
img = shot.copy(); d = ImageDraw.Draw(img)
font = ImageFont.truetype('assets/fonts/BarlowCondensed-Bold.ttf', int(13 * S))
def badge(x, y, txt, col):
    cx, cy, r = x * S, y * S, 9 * S
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=col, outline=(0, 0, 0), width=max(1, int(S)))
    w = d.textlength(txt, font=font); d.text((cx - w / 2, cy - 8 * S), txt, fill=(11, 11, 13), font=font)
for i, c in enumerate(C, 1): badge(c[1][0], c[1][1], str(i), (255, 123, 177))
for i, (n, k, t) in enumerate(D):
    r = cv[k]['r']; badge(r[0] + r[2] - 10, r[1] + 10, chr(65 + i), (2, 235, 186))
img.save('docs/manual/tour.png')
def crop(name, box, src=img): src.crop(tuple(int(v * S) for v in box)).save('docs/manual/%s.png' % name)
crop('header', (52, 0, 1308, 46)); crop('displays', (52, 46, 1184, 202)); crop('level', (52, 202, 372, 590)); crop('detector', (362, 202, 640, 590))
crop('time', (630, 202, 948, 590)); crop('meter', (938, 202, 1184, 590)); crop('meters', (1184, 46, 1308, 590))
e = html.escape
rows = '\n'.join('<tr><td><span class="b">%d</span></td><th>%s</th><td>%s</td><td>%s</td><td>%s</td></tr>' % (i, e(n), e(t), e(r), e(df)) for i, (n, a, r, df, t) in enumerate(C, 1))
drows = '\n'.join('<tr><td><span class="b g">%s</span></td><th>%s</th><td colspan="3">%s</td></tr>' % (chr(65 + i), e(n), e(t)) for i, (n, k, t) in enumerate(D))
import re
fac = re.findall(r'mk \("([^"]+)", "([^"]+)"', open('src/editor/Presets.cpp').read())
frows = ''.join('<tr><th>%s</th><td>%s</td></tr>' % (e(a), e(b)) for a, b in fac)
page = open('tools/manual_template.html').read()
for k, v in {'@VER@': ver, '@ROWS@': rows, '@DROWS@': drows, '@FAC@': frows}.items(): page = page.replace(k, v)
open('docs/manual.html', 'w').write(page)
print('manual ok', len(C), 'controls')
