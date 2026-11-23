// Landscape relief. Only the screen callback reads or writes this geometry.
// t and knob fractions are Q10. Heights are in the audition's 960 x 300 space.
#pragma once
#include "landscape_data.h"
namespace kiln { namespace relief {
using namespace opxy::draw;
struct Knobs { int a,b,c,d,warm,wet; };
static inline int frac(int x,int u){return x*u/1024;}
static inline int bellAt(int t,int centre,int width){
    const int distance=t>centre?t-centre:centre-t;
    const int index=distance*32/width;
    return index<128?bell[index]:0;
}
static inline int peak(int t,int height,int centre,int width){return frac(height,bellAt(t,centre,width));}
static inline int contour(unsigned mode,int t,int k,const Knobs& p){
    const int a=p.a,b=p.b,c=p.c,d=p.d;
    int y=0;
    switch(mode){
    case Collision:
        y=24+peak(t,45+frac(48,a),195+frac(184,b),143)
            +peak(t,46+frac(66,c),594-frac(154,b),123+frac(123,d));break;
    case Space:
        y=18+peak(t,100-7*k,164+frac(102,c)+k*(12+frac(36,a)),56+frac(164,1024-b))
            +peak(t,24+frac(33,1024-d),563+41*k,143);break;
    case Formant:{
        const int w=87-frac(53,c),offset=frac(143,b-512);
        y=18+peak(t,68+frac(23,a),174+offset+frac(123,a),w+26)
            +peak(t,101-frac(38,a),461+offset+k*frac(51,d),w)
            +peak(t,28+frac(67,a),748+offset-frac(72,a),w);break;}
    case Gate:{
        const int cycles=c<256?2:c<768?4:8;
        y=26+peak(t,73,317,246)+frac(frac(13,d),sine(frac(1800,t)))+frac(13,sine(900+frac(1800*cycles,t)));break;}
    case Dirt:{
        y=frac(25+peak(t,93,307,167)+peak(t,32,543,266),696+frac(410,a))
            +peak(t,22+frac(34,a),256+frac(369,b),102+frac(113,c));
        const int steps=8+frac(12,d);
        if(t>348)y+=frac(frac(40,(t*steps/1024)*1024/steps-t)-frac(13,sine((t*27/1024)*1375)),d);
        break;}
    case Memory:
        y=18+peak(t,65+frac(40,a),236+frac(225,d-512),113+frac(102,b))
            +peak(t,24+frac(55,b),379+61*k+frac(123,c),113+frac(184,c));break;
    case Tuned:{
        const int phase=frac(5730+frac(12605,b),t)-1146;
        // exp(-x), sampled at 1/16 steps from x=0 to x=3.5.
        static const unsigned short decay[]={1024,962,904,849,798,749,704,661,621,583,548,515,484,455,427,401,377,354,332,312,293,275,259,243,228,214,202,189,178,167,157,147,139,130,122,115,108,101,95,90,84,79,74,70,65,62,58,54,51,48,45,42,40,37,35,33,31};
        const int z=frac(512+3*d,t)/64;
        y=24+peak(t,44+frac(48,a),225,143)+frac(frac(frac(58,307+frac(717,c)),decay[z]),(1024+sine(phase+900))/2);break;}
    default:
        y=25+peak(t,93,307,167)+peak(t,32,543,266)+peak(t,14+frac(28,a),317,87+frac(143,b))-frac(frac(33,c),t);break;
    }
    return 26+frac((y<8?8:y)-26,p.wet);
}
struct Edge {int x,y,depth;};
static inline Edge crest(unsigned mode,int layer,int t,const Knobs& p){
    const int lift=(3-layer)*(4+(mode==Spall?frac(15,p.a):mode==Space?frac(14,p.c):8));
    int h=contour(mode,t,layer,p);
    if(p.warm){
        const int mean=(contour(mode,clamp(t-44,0,1024),layer,p)+contour(mode,clamp(t+44,0,1024),layer,p))/2;
        h+=frac(frac(mean-h,870),p.warm);
    }
    const int length=mode==Spall?575+frac(225,p.d)-28*layer:804-35*layer;
    return {20+37*layer+frac(length,t),
            52+(208+18*layer-lift-frac(h,1024-72*layer))*4/5,
            frac(34,1024-frac(881,t))};
}
// Four world-space plates of up to 430 px fit the local +/-64 path domain
// using a fixed parent matrix. local x=(sceneX-480)/8, y=(sceneY-150)/4.
static inline Point local(int x,int y){return {(x-480)*128,(y-150)*256};}
static inline void gateTiles(Frame& f,int row,const Knobs& p,unsigned mask){
    for(unsigned i=0;i<16;++i){
        const unsigned node=44+row*16+i;
        const bool on=(mask&(0x8000u>>i))!=0;
        if(!on && p.b>=1019){hide(f,node);continue;}
        const int t=(2*i+1)*32,k=row+2;
        const Edge e=crest(Gate,k,t,p);
        const int span=(804-35*k)/16;
        const int scale=(span-3-frac(8,p.d))*512/22;
        place(f,node,(e.x-span/2+2)*512,(e.y+(on?0:frac(19,p.b)))*512+42*unit,
              scale,on?2+row:1,fill_and_stroke);
    }
}
} }
#include "rider.h"
namespace kiln { namespace relief {
static inline void draw(DisplayState& state,const Input& in,Frame& f,unsigned mode,const unsigned (&v)[4],unsigned pat){
    unsigned masks[2]={};
    if(mode==Gate)for(unsigned ch=0;ch<2;++ch)
        masks[ch]=gateMask(pat,ch,in.choices[ch+1],choice(in.slots[6+ch],128),in.choices[3],pat&3);
    const bool warm=mode==Dirt||mode==Collision||mode==Spall;
    const unsigned w=warm?knob(in.slots[4])>>8:0,wet=knob(in.slots[5])>>8;
    const unsigned keys[]={((v[0]>>8)<<24)|((v[1]>>8)<<16)|((v[2]>>8)<<8)|(v[3]>>8),
                            (mode<<16)|(w<<8)|wet,(masks[0]<<16)|masks[1]};
    const Knobs p={int((v[0]>>8)*4),int((v[1]>>8)*4),int((v[2]>>8)*4),int((v[3]>>8)*4),int(w*4),int(wet*4)};
    const bool rebuild=!state.revision||state.last[0]!=keys[0]||state.last[1]!=keys[1]||state.last[2]!=keys[2];
    if(state.revision && (state.last[1]>>16)!=mode) {
        const unsigned word=state.ride.word;
        state.ride=Ride{};state.ride.word=word;
    }
    const bool step=advance(state.ride,in,mode);
    if(!rebuild){if(step)moveLandscape(state,f,mode,p);return;}
    for(unsigned i=0;i<3;++i){state.last[i]=keys[i];state.ride.lift[i]=0;}
    if(++state.revision==0)++state.revision;
    for(unsigned i=12;i<Frame::node_count;++i)hide(f,i);
    for(unsigned i=glyphs[mode];i<glyphs[mode+1];++i)place(f,i,29*unit,204*unit,unit,0,keep);
    for(int k=0;k<4;++k){
        auto& line=f.paths[k];line.key=state.revision;
        place(f,k,60*unit,((42*2+150)*unit)/4,unit,0,keep);
        Edge previous={};
        for(int j=0;j<32;++j){
            const int t=j*1024/31;
            const Edge e=crest(mode,k,t,p);
            if(j==0 && (mode!=Gate||k<2)){
                const int height=52+(208+18*k+24)*4/5-e.y;
                place(f,89+k,e.x*512,42*unit+e.y*512,clamp(height*512/32,256,2*unit),0,keep);
            }
            line.points[j]=local(e.x,e.y);
            line.points[63-j]=local(e.x+12,e.y+e.depth);
            if(k==3 && j && mode!=Gate){
                const int y=(previous.y+e.y)/2+(previous.depth+e.depth)/4;
                const int width=(e.x-previous.x)*512;
                place(f,12+j-1,(previous.x+8)*512,42*unit+y*512,clamp((width+1400)/16,1,2*unit),0,keep);
            }
            previous=e;
        }
        if(mode==Gate && k>=2){hide(f,k);gateTiles(f,k-2,p,masks[k-2]);}
    }
    moveLandscape(state,f,mode,p);
}
} }
