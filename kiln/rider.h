// Native display motion. Elapsed time is Q10 seconds. The full ridge contours
// remain cached while the display moves their nodes and reads their top edges.
#pragma once
#include "rider_art.h"
namespace kiln { namespace relief {
using namespace opxy::draw;
inline int visualLevel(opxy::u32 word) {
    // Q31 return peak. One doubling of amplitude occupies 16 display units.
    // Bits 0..7 belong to the meter sequence, not the signal.
    const opxy::u32 peak=word & 0x7fffff00u;
    if(peak<=0x10000u)return 0;
    opxy::u32 n=peak;
    int octave=0;
    if(n>=1u<<16){n>>=16;octave+=16;}
    if(n>=1u<<8){n>>=8;octave+=8;}
    if(n>=1u<<4){n>>=4;octave+=4;}
    if(n>=1u<<2){n>>=2;octave+=2;}
    if(n>=2)++octave;
    return (octave-16)*16+static_cast<int>((peak>>(octave-4))&15u);
}
inline bool advance(Ride& r,const Input& in,unsigned mode) {
    if(in.reduced_motion) {
        const bool moving=r.power||r.energy||r.slow;
        const unsigned word=r.word;
        r.power=r.energy=r.slow=r.cooldown=r.age=r.ticks=0;
        r.word=r.previous=word;r.pending=0;r.stale=4;r.primed=true;
        return moving;
    }
    // Observe each screen frame, including those between 20 Hz simulation
    // steps. A following zero must not erase a short hit before it is drawn.
    const bool fresh=r.primed && r.previous!=r.word;
    if(!r.primed)r.stale=4;
    if(fresh) {
        r.stale=0;
        const unsigned peak=r.word & 0x7fffff00u;
        if(peak>r.pending)r.pending=peak;
    }
    r.previous=r.word;r.primed=true;
    r.ticks+=clamp(in.elapsed,0,128);
    if(r.ticks<51)return false;
    const int steps=r.ticks/51;r.ticks%=51;
    const unsigned current=r.stale<3?r.word:0;
    const int level=visualLevel(r.pending>current?r.pending:current);
    r.pending=0;
    r.stale=clamp(r.stale+steps,0,4);
    const bool moving=r.energy||r.slow||r.power||level;
    for(int i=0;i<steps;++i) {
        const int attack=level-r.slow;
        r.energy+=(level-r.energy)/(level>r.energy?2:4);
        r.slow+=(level-r.slow)/6;
        if(!level){if(r.energy<4)r.energy=0;if(r.slow<6)r.slow=0;}
        if(r.cooldown>0)--r.cooldown;
        if(!r.cooldown&&attack>22&&level>=64) {
            r.power=clamp(60+2*attack,0,255);
            r.span=(mode==Spall?9:5)+(mode==Spall?4:5)*r.power/255;
            r.age=0;r.cooldown=r.span+4;
        } else if(r.power&&++r.age>=r.span) { r.power=0;r.age=0; }
    }
    return moving;
}
inline Point topAt(const Frame& f,int k,int t) {
    const int a=clamp(t,0,1024)*31,j=clamp(a>>10,0,30),u=a-j*1024;
    const auto& line=f.paths[k];
    const Point l=line.points[j],r=line.points[j+1];
    return {240*unit+4*(l.x+(r.x-l.x)*u/1024),
            117*unit+2*(l.y+(r.y-l.y)*u/1024)};
}
inline void moveLandscape(DisplayState& s, Frame& f,unsigned mode,const Knobs& p) {
    Ride& r=s.ride;
    const int lift[]={r.energy*22,r.slow*14,r.slow*6};
    for(int k=0;k<3;++k){
        const int delta=lift[k]-r.lift[k];
        if(delta) {
            if(mode!=Gate||k<2){f.nodes[k].y-=delta/2;f.nodes[89+k].y-=delta;}
            else for(int j=44;j<60;++j)if(f.nodes[j].x!=hidden_x*unit)f.nodes[j].y-=delta;
        }
        r.lift[k]=lift[k];
    }
    const int progress=r.power?r.age*1024/r.span:0;
    const int eased=(progress*progress/1024)*(3072-2*progress)/1024;
    const int air=4*progress*(1024-progress)/1024*r.power/255;
    const int locations[]={389,225,471,307,532,614,369,307+frac(164,p.b)};
    const int riderT=locations[mode]+(mode==Spall?0:(r.slow*87+r.energy*33)/255);
    if(mode==Spall) {
        const int spacing=297+frac(123,p.b);
        const int origin=riderT+spacing/2-frac(spacing,eased);
        for(int k=0;k<4;++k)for(int j=0;j<5;++j){
            const int index=94+k*5+j, t=origin+(j-2)*spacing;
            if(t<4||t>1018){hide(f,index);continue;}
            const Point at=topAt(f,k,t);
            const int y=at.y-(k<3?r.lift[k]:0);
            const int height=k==3?187*unit-y:(34-frac(29,t))*unit/2;
            if(height<unit){hide(f,index);continue;}
            const int scale=clamp(height/64,32,2*unit);
            place(f,index,at.x,y,scale,0,keep);
            // Fall angles the cleavage; Fracture opens the front-face cut.
            turn(f,index,rotation(-frac(65+frac(65,p.a),frac(p.c,p.wet))));
        }
    }
    const Point at=topAt(f,0,riderT), l=topAt(f,0,riderT-15), rr=topAt(f,0,riderT+15);
    const int tilt=clamp((rr.y-l.y)*550/(rr.x-l.x),-360,360);
    const int pose=air>285?2:r.energy>99?1:0;
    const int y=clamp(at.y-lift[0]-air*(mode==Spall?15:10),59*unit,185*unit);
    for(int i=0;i<3;++i)for(unsigned n=poseNodes[i];n<poseNodes[i+1];++n){
        if(i!=pose){hide(f,n);continue;}
        place(f,n,at.x,y,unit,0,keep);
        turn(f,n,rotation(tilt-(pose==2?48:0)));
    }
}
} }
