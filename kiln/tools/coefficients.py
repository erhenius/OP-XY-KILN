"""Regenerate the fixed-point lookup tables for 44.1 kHz."""
from math import sin,cos,exp,pi
from pathlib import Path
d=Path(__file__).resolve().parents[1]
sr=44100
Q=1<<24
pole=lambda f:1-exp(-2*pi*f/sr)
tables={}
def curve(name,fn,n=128):tables[name]=[round(Q*fn(i/n)) for i in range(n+1)]
curve('clipSmall',lambda x:4*x/(1+4*x),256)
curve('clipLarge',lambda x:(4+64*x)/(5+64*x),256)
curve('invEnv',lambda x:1/(.012+x),512)
curve('ringLinear',lambda x:10**(27*x/20)/(1+.12*10**(27*x/20)))
curve('drive27',lambda x:10**(27*x/20))
curve('drive24',lambda x:10**(24*x/20))
curve('tiltGain',lambda x:10**((18*x-9)/40))
curve('splitPole',lambda x:pole(65*2**(5*x)))
curve('colourPole',lambda x:pole(450*2**(4.8*x)))
curve('dampSpace',lambda x:pole(11500*2**(-4.5*x)))
curve('dampRing',lambda x:pole(12000*2**(-4.3*x)))
curve('forget',lambda x:exp(-1/(sr*.020*60**x)))
curve('gateEdge',lambda x:1-exp(-1/(sr*(.001+.048*x*x))))
curve('tune',lambda x:2**((round(x*24)-12)/12),24)
curve('sine',lambda x:sin(2*pi*8000*x/sr),256)
curve('cosine',lambda x:cos(2*pi*8000*x/sr),256)
tables['periodQ12']=[round(sr*4096/(440*2**((36+i-69)/12))) for i in range(37)]
out=['// Q24 lookup tables, except periodQ12 (samples in Q12).','#pragma once','#include <opxy/types.hpp>','namespace kiln { namespace lut {']
for name,values in tables.items():
    out.append(f'const opxy::i32 {name}[] = {{')
    out.extend('    '+', '.join(map(str,values[i:i+10]))+',' for i in range(0,len(values),10))
    out.append('};')
out.append('}}')
(d/'tables.h').write_text('\n'.join(out)+'\n')
