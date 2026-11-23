#include "core.h"
#include "tables.h"
#include <opxy/q31.hpp>

namespace kiln {
namespace {
// Q24 audio leaves headroom for the drive and resonator arithmetic. Q31 only at the bus.
constexpr i32 unit=1<<24;
constexpr i32 q(double v){return static_cast<i32>(v*unit+(v<0?-.5:.5));}
inline i32 mul(i32 a,i32 b){return static_cast<i32>(opxy::q31::wide(a,b)>>24);}
inline i32 bounded(i32 x,i32 low,i32 high){return x<low?low:x>high?high:x;}
inline i32 mag(i32 x){return x<0?-x:x;}
inline i32 blend(i32 a,i32 b,i32 t){return a+mul(b-a,t);}
inline i32 follow(i32& current,i32 target,i32 coeff){
    current=blend(current,target,coeff);return current;
}
inline i32 tab(const i32* t,unsigned index,unsigned fraction){
    return t[index]+mul(t[index+1]-t[index],static_cast<i32>(fraction));
}
inline i32 curve(const i32* t,unsigned knob16){return tab(t,knob16>>9,(knob16&511)<<15);}
inline i32 shape(i32 x){
    const unsigned a=static_cast<unsigned>(mag(x));
    i32 y;
    if(a<4u*unit)y=tab(lut::clipSmall,a>>18,(a&262143)<<6);
    else if(a<68u*unit){const unsigned v=a-4u*unit;y=tab(lut::clipLarge,v>>22,(v&4194303)<<2);}
    else y=lut::clipLarge[256];
    return x<0?-y:y;
}
inline i32 toBus(i32 x){return opxy::q31::shift_left(x,7);}
inline i32 norm(unsigned v){return static_cast<i32>(v<<8);}
inline i32 trig(const i32* table,unsigned hz){
    const unsigned scaled=(hz*8192)/1000; // 8000 Hz covers 65536 table units
    return tab(table,scaled>>8,(scaled&255)<<16);
}
inline Resonator band(unsigned hz,unsigned quality256){
    const i32 alpha=trig(lut::sine,hz)/(2*static_cast<i32>(quality256))*256;
    const i32 g=shape(alpha);
    return {2*mul(trig(lut::cosine,hz),unit-g),2*g-unit,g};
}
inline i32 resonate(const Resonator& c,i32 difference,i32& y1,i32& y2){
    const i32 y=mul(c.g,difference)+mul(c.a,y1)+mul(c.b,y2);
    y2=y1;y1=y;return y;
}
inline i32 tap(const i32* data,const State& s,unsigned distance,unsigned mask){
    return distance<=s.valid?data[(s.write-distance)&mask]:0;
}
inline i32 readPeriod(const i32* data,const State& s,unsigned period){
    const unsigned d=period>>12;
    return blend(tap(data,s,d,2047),tap(data,s,d+1,2047),(period&4095)<<12);
}
#include "level_match.h"
#include "spall.h"
struct Setup {
    i32 v[4],outputGain,drive,cutoff,colour,tilt,invTilt,damping,recovery,edge,warm,depth,bodyPole;
    unsigned delays[4],periods[2],gate[2],rate,inc,vowel,pitch,quality;
    Resonator bands[3],shell;
};
// Wet-output gain at five evenly spaced Drive positions. Calibrated against
// bypass on bass, leads, pads, plucks, percussion and mixes (see QA.md).
// These are linear amplitude ratios, interpolated once per block.
inline i32 driveTrim(const i32 (&gains)[5],unsigned raw){
    const unsigned segment=raw>>14;
    return blend(gains[segment],gains[segment+1],(raw&16383u)<<10);
}
inline i32 outputGain(unsigned mode,i32 driveWord){
    static const i32 collision[]={unit,q(.82),q(.56),q(.36),q(.22)};
    static const i32 dirt[]={q(.75),q(.334),q(.19),q(.143),q(.115)};
    static const i32 memory[]={unit,q(.505),q(.29),q(.185),q(.129)};
    static const i32 tuned[]={q(1.12),q(1.10),unit,q(.72),q(.45)};
    const unsigned raw=knob(driveWord);
    switch(mode){
    case Collision:return driveTrim(collision,raw);
    case Dirt:return driveTrim(dirt,raw);
    case Memory:return driveTrim(memory,raw);
    case Tuned:return driveTrim(tuned,raw);
    case Space:return q(1.19);
    case Formant:return q(1.33);
    default:return unit;
    }
}
void setup(Setup& c,unsigned m,opxy::Parameters p,const opxy::Transport& t){
    c.outputGain=outputGain(m,p[0]);
    unsigned v[4];
    for(unsigned j=0;j<4;++j){v[j]=mapped(m,j,p[j]);c.v[j]=norm(v[j]);}
    c.warm=norm(knob(p[4]));c.depth=bounded(2*c.v[0],0,unit);
    if(m==Space){
        const unsigned size=794u+(2073u*v[0]+32767)/65535;
        const unsigned fraction[4]={35,52,77,100};
        for(unsigned i=0;i<4;++i)c.delays[i]=(size*fraction[i]+50)/100;
        c.damping=curve(lut::dampSpace,v[3]);return;
    }
    if(m==Formant){
        c.vowel=v[0]*4;c.pitch=lut::tune[rounded(v[1],24)];c.quality=512+((v[2]*3072)>>16);return;
    }
    if(m==Gate){
        const unsigned preset=rounded(v[0],7);
        for(unsigned ch=0;ch<2;++ch)c.gate[ch]=gateMask(preset,ch,choice(p[9+ch],128),choice(p[6+ch],128),choice(p[11],4),preset&3);
        c.rate=rounded(v[2],2);c.edge=curve(lut::gateEdge,v[3]);
        unsigned bpm=t.bpm_x10?t.bpm_x10:1200;
        c.inc=(bounded(bpm,100,4000)*6341+5000)/10000;return;
    }
    c.drive=curve(m==Memory?lut::drive24:lut::drive27,v[0]);
    c.bodyPole=m==Collision?q(.034992):m==Dirt?q(.051355):q(.043204);
    if(m==Collision){c.cutoff=curve(lut::splitPole,v[1]);c.colour=curve(lut::colourPole,v[3]);}
    if(m==Dirt){c.tilt=curve(lut::tiltGain,v[1]);c.invTilt=curve(lut::tiltGain,65535-v[1]);}
    if(m==Memory)c.recovery=curve(lut::forget,v[2]);
    if(m==Tuned){
        c.periods[0]=lut::periodQ12[rounded(v[1],36)];c.periods[1]=(2*c.periods[0])/3;
        const unsigned shellHz=(44100u*2048+c.periods[0]/2)/c.periods[0];
        c.shell=band(shellHz,461+((v[2]*1152)>>16));
        c.damping=curve(lut::dampRing,v[3]);
    }
}
inline void formants(Setup& c,i32 attack){
    const unsigned vowels[5][3]={{800,1150,2900},{530,1850,2500},{300,2200,3000},{520,850,2450},{330,780,2150}};
    const unsigned pos=bounded(c.vowel+(mul(mul(attack,c.v[3]),q(1.7))>>8),0,4<<16);
    const unsigned a=pos>>16,b=a<4?a+1:4;
    for(unsigned j=0;j<3;++j){
        const int f=vowels[a][j]+((static_cast<int>(vowels[b][j])-static_cast<int>(vowels[a][j]))*static_cast<int>(pos&65535))/65536;
        const unsigned hz=static_cast<unsigned>((opxy::q31::wide(f,c.pitch))>>24);
        c.bands[j]=band(hz,c.quality);
    }
}
inline i32 warm(ChannelState& s,i32 grit,i32 mix){
    if(!mix)return grit;
    return blend(grit,follow(s.edge,grit,q(.23171)),mix);
}
inline i32 body(ChannelState& s,i32 x,i32 pole){
    const i32 lo=follow(s.bodyLow,x,pole);
    return lo-follow(s.sub,lo,q(.00625));
}
template<Mode mode>
void render(State& s,Setup& c,opxy::Buffer buffer,opxy::Channel left,opxy::Channel right,
            i32 mix,bool changed){
    BlockLevel level(s.level);
    const i32 from[2]={s.lastOut[0],s.lastOut[1]};
    for(unsigned n=0;n<64;++n){
        const i32 in[2]={left[n]>>7,right[n]>>7};
        i32 attack=0,edgeMix=0;
        if(mode==Dirt||mode==Memory||mode==Space||mode==Formant||mode==Collision){
            const i32 al=mag(in[0]),ar=mag(in[1]),peak=al>ar?al:ar;
            if(mode!=Collision||c.warm)follow(s.fast,peak,peak>s.fast?q(.02254):q(.001708));
            if(mode==Memory)s.history=bounded(mul(s.history,c.recovery),s.fast,unit);
            if(mode==Dirt||mode==Space||mode==Formant){
                follow(s.slow,peak,q(.0004273));
                const unsigned env=static_cast<unsigned>(bounded(s.slow,0,unit-1));
                const i32 inv=tab(lut::invEnv,env>>15,(env&32767)<<9);
                // Clamp the numerator before multiplying to keep the ratio bounded.
                const i32 delta=bounded(s.fast-s.slow,0,s.slow+q(.012));
                attack=bounded(mul(mul(delta,inv),q(1.1)),0,unit);
            }
            if((mode==Dirt||mode==Collision)&&c.warm)
                edgeMix=mul(mul(c.warm,c.depth),mul(q(.96),bounded(mul(s.fast,q(3.8)),0,unit)));
        }
        if(mode==Formant && !(n&31))formants(c,attack);
        const i32 wetMix=s.mix+static_cast<i32>((opxy::q31::wide(mix-s.mix,n+1))>>6);
        for(unsigned ch=0;ch<2;++ch){
            ChannelState& a=s.ch[ch];const i32 x=in[ch];
            i32 y=0;
            if(mode==Space){
                const i32 weight=blend(unit-attack,attack,c.v[1]);
                const i32 mid=(in[0]+in[1])/2;
                i32* data=buffer.words+ch*4096;
                data[s.write&4095]=follow(a.shade,mul(blend(x,mid,q(.35)),q(.13)+mul(q(1.55),weight)),c.damping);
                const i32 weights[4]={q(.61),q(.45),q(.35),q(.27)};
                for(unsigned j=0;j<4;++j){
                    const i32 w=mul(c.v[2],q(.95));
                    y+=mul(tap(data,s,c.delays[j],4095),mul(weights[j],((j+ch)&1)?unit+w:unit-w));
                }
            }else if(mode==Formant){
                const i32 weights[3]={unit,q(.84),q(.55)};
                for(unsigned j=0;j<3;++j)y+=mul(weights[j],resonate(c.bands[j],x-a.previous2,a.y1[j],a.y2[j]));
                a.previous2=a.previous;a.previous=x;y=mul(y,q(2.5));
            }else if(mode==Gate){
                const unsigned step=(s.phase>>(23-c.rate))&15;
                const i32 target=c.gate[ch]&(0x8000u>>step)?unit:unit-c.v[1];
                y=mul(x,follow(a.gate,target,c.edge));
            }else if(mode==Collision){
                const i32 lo=follow(a.low,x,c.cutoff);
                const i32 residue=shape(mul(c.drive,x))-shape(mul(c.drive,lo))-shape(mul(c.drive,x-lo));
                const i32 grit=follow(a.shade,warm(a,residue,edgeMix),c.colour);
                const i32 base=shape(mul(q(2.6),body(a,x,c.bodyPole)));
                y=x+mul(c.v[2],mul(q(2.8),grit)+mul(mul(q(1.25),c.v[0]),base)-mul(q(2.1),follow(a.previous,grit,c.bodyPole)));
            }else if(mode==Dirt){
                const i32 lo=follow(a.low,x,q(.11405));
                const i32 tilted=mul(lo,c.invTilt)+mul(x-lo,c.tilt);
                const i32 weight=blend(unit-attack,attack,c.v[2]);
                // Clip outside the useful range of the waveshaper before applying the last gain.
                const i32 driven=bounded(mul(c.drive,tilted),-36*unit,36*unit);
                s.seed=s.seed*1664525u+1013904223u;
                const i32 noise=static_cast<i32>(s.seed>>7)-unit;
                const i32 dust=mul(mul(noise-follow(a.noise,noise,q(.18)),s.fast),mul(q(.82),c.v[3]));
                y=mul(q(.83),shape(mul(driven,q(.45)+mul(q(2.8),weight))))+dust;
                const i32 direct=mul(q(.83),tilted);
                y=direct+warm(a,y-direct,edgeMix);
                const i32 sustain=mul(unit-attack,q(.42)+mul(q(.58),unit-c.v[2]));
                y+=mul(mul(c.v[0],q(.5)+mul(q(1.8),sustain)),shape(mul(q(4.5),body(a,x,c.bodyPole))));
            }else if(mode==Memory){
                const i32 mem=mul(bounded(2*s.history,0,unit),c.v[1]);
                const i32 bias=mul(2*c.v[3]-unit,q(.09)+mul(q(1.9),mem));
                const i32 gain=mul(c.drive,unit+mul(q(2.4),mem));
                y=mul(shape(mul(gain,x)+bias)-shape(bias),q(.96)-mul(q(.49),mem));
                const i32 rebound=bounded(3*(s.history-s.fast),0,unit);
                y+=mul(mul(c.v[0],q(.52)+mul(c.v[1],q(.58)+mul(q(2.1),rebound))),shape(mul(q(4.8),body(a,x,c.bodyPole))));
            }else if(mode==Tuned){
                const i32 residue=shape(mul(c.drive,x))-mul(c.invTilt,x);
                i32 sum=0;
                for(unsigned j=0;j<2;++j){
                    i32* line=buffer.words+(ch*2+j)*2048;
                    const i32 delayed=readPeriod(line,s,c.periods[j]);
                    const i32 damped=follow(a.feedback[j],delayed,c.damping);
                    line[s.write&2047]=bounded(mul(residue,q(.4))+mul(q(.3)+mul(q(.675),c.v[2]),damped),-12*unit,12*unit);
                    sum+=j?mul(q(.6),damped):damped;
                }
                const i32 excite=x+mul(q(.4)+2*c.v[0],follow(a.shade,mag(residue),c.bodyPole));
                const i32 shell=resonate(c.shell,excite-a.previous2,a.y1[0],a.y2[0]);
                a.previous2=a.previous;a.previous=excite;
                y=mul(q(.73),x)+mul(q(.84),sum)+mul(mul(c.v[2],q(2.2)+3*c.v[0]),shell);
            }
            if(mode==Collision||mode==Dirt||mode==Memory||mode==Tuned)y-=follow(a.dc,y,q(.000871));
            if(mode!=Gate)y=mul(y,c.outputGain);
            if(mode!=Gate)y=level.apply(s.level,n,ch,x,y);
            i32 sample=blend(x,y,wetMix);
            if(changed)sample=blend(from[ch],sample,(n+1)<<18);
            (ch?right:left)[n]=toBus(sample);
            s.lastOut[ch]=bounded(sample,-unit,unit-1);
        }
        if(mode==Space||mode==Tuned){++s.write;if(s.valid<4096)++s.valid;}
        if(mode==Gate)s.phase=(s.phase+c.inc)&0x07ffffffu;
    }
    // A quiet source can excite two nearly undamped combs in Tuned Dirt.
    if(mode!=Gate)level.finish(s.level,mode==Tuned?16:256);
    s.mix=mix;
}
}
void initialize(State& s){s=State{};s.seed=5309;s.active=-1;s.shards.armed=true;s.shards.quietBlocks=8;s.level.gain=s.level.target=unit;}
void process(State& s,opxy::Buffer buffer,const opxy::Transport& t,opxy::Channel l,opxy::Channel r,opxy::Parameters p){
    if(!buffer.words||buffer.count<8192){for(unsigned i=0;i<64;++i)l[i]=r[i]=0;return;}
    const unsigned mode=choice(p[8],8);
    const bool changed=s.active!=static_cast<int>(mode);
    const i32 mix=norm(knob(p[5]));
    if(changed){
        const i32 oldL=s.lastOut[0],oldR=s.lastOut[1];
        initialize(s);s.active=mode;s.lastOut[0]=oldL;s.lastOut[1]=oldR;s.mix=mix;
    }
    if(mode==Spall){
        spallBlock(s,buffer,l,r,p,mix,changed);
        return;
    }
    Setup c={};setup(c,mode,p,t);
    if(mode==Gate && t.playing){
        const u32 anchor=((t.ticks96%768)<<19)/3;
        const i32 error=static_cast<i32>((anchor-s.phase+(1u<<26))&0x07ffffffu)-(1<<26);
        if(!s.playing || (t.ticks96!=s.lastTick && mag(error)>(1<<18)))s.phase=anchor;
    }
    s.playing=t.playing;s.lastTick=t.ticks96;
    switch(mode){
    case Collision: render<Collision>(s,c,buffer,l,r,mix,changed);break;
    case Space: render<Space>(s,c,buffer,l,r,mix,changed);break;
    case Formant: render<Formant>(s,c,buffer,l,r,mix,changed);break;
    case Gate: render<Gate>(s,c,buffer,l,r,mix,changed);break;
    case Dirt: render<Dirt>(s,c,buffer,l,r,mix,changed);break;
    case Memory: render<Memory>(s,c,buffer,l,r,mix,changed);break;
    case Tuned:
        // drive / (1 + .12 * drive), precomputed for the block.
        c.invTilt=curve(lut::ringLinear,mapped(mode,0,p[0]));
        render<Tuned>(s,c,buffer,l,r,mix,changed);break;
    }
}
}
