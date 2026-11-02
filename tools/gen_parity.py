#!/usr/bin/env python3
"""Run the approved mockup's own JS engine on fixed scenarios and write tests/fixtures/mockup-parity.txt.
tests/test_core.cpp runs the C++ core on the same input and compares gain reduction (parity check).
Run from the repo root: python3 tools/gen_parity.py"""
import asyncio, math
from playwright.async_api import async_playwright
src = open('docs/mockup/Apurrr2500.html').read()
open('/tmp/apurr_mock_wrapped.html', 'w').write('<!doctype html><html><head><meta charset="utf-8"></head><body>' + src + '</body></html>')
# scenario: st fields of the mockup (see DEF in the page)
SC = [
 ('default', dict()),
 ('old_loud_tilt', dict(thr=0, ratio=3, attack=3, release=1, knee=2, tilt=2, type=0, link=3, shape=2)),
 ('parallel_manual', dict(tilt=1, blend=2, mix=40, muMode=1, gain=6, ratio=4, thr=2)),
 ('levelmatch_soft', dict(levelMatch=1, thr=-6, ratio=4, knee=0, attack=2, release=0, link=0)),
]
JS = r"""
(scn)=>{const A=window.__apurr;const E=A.eng.constructor;const res={};
 for(const [name,over] of scn){
  Object.assign(A.st,{bypass:0,levelMatch:0,meter:0},{thr:4,ratio:1,attack:5,release:2,relVar:150,knee:1,tilt:0,type:1,extSc:0,link:6,shape:0,muMode:0,gain:0,blend:0,mix:100,inSw:1},over);
  const e=new E();e.prepare(48000);e.setAvg(300);const q=A.core();e.setP(q,true);e.curMu=e.tMu;
  const N=48000,B=48,gl=[],gr=[];const L=new Float32Array(B),R=new Float32Array(B);let sq=0,cnt=0;
  for(let b=0;b<N/B;b++){
    for(let i=0;i<B;i++){const k=b*B+i,t=k/48000,db=t<.2?-30:t<.5?-8:-24,a=Math.pow(10,db/20);
      const s=a*(Math.sin(2*Math.PI*200*t)+.5*Math.sin(2*Math.PI*3100*t))/1.5*1.414;L[i]=s;R[i]=s*.7}
    e.process(L,R,B,null);gl.push(e.grL);gr.push(e.grR);
    if(b>=N/B-200){for(let i=0;i<B;i++){sq+=L[i]*L[i];cnt++}}}
  res[name]={core:q,gl,gr,outRms:10*Math.log10(sq/cnt)}}
 return res}
"""
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(); pg = await b.new_page()
        await pg.goto('file:///tmp/apurr_mock_wrapped.html'); await pg.wait_for_timeout(800)
        d = await pg.evaluate(JS, SC)
        out = []
        for name, over in SC:
            r = d[name]
            st = dict(thr=4, ratio=1, attack=5, release=2, relVar=150, knee=1, tilt=0, type=1, extSc=0, link=6, shape=0, muMode=0, gain=0, blend=0, mix=100, inSw=1, levelMatch=0); st.update(over)
            out.append('scenario %s' % name)
            out.append('st ' + ' '.join('%s=%g' % (k, st[k]) for k in sorted(st)))
            out.append('outRms %.4f' % r['outRms'])
            out.append('grL ' + ' '.join('%.4f' % v for v in r['gl']))
            out.append('grR ' + ' '.join('%.4f' % v for v in r['gr']))
        open('tests/fixtures/mockup-parity.txt', 'w').write('\n'.join(out) + '\n')
        print('written', len(out) // 5, 'scenarios')
        await b.close()
asyncio.run(main())
