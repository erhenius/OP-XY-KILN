#include "../fx.cpp"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

static_assert(sizeof(kiln::State)<=468,"The buffered, shift-row effect must fit the native slot.");
using opxy::i32;
using opxy::u32;
namespace {
struct Fixture {
    kiln::State s;
    i32 samples[8192],l[64],r[64];
    i32 p[12]={1<<30,1<<30,1<<30,1<<30,0,0x7fffffff,0,0,0,0,0,0};
    u32 block=0,seed=17;
    Fixture(){kiln::initialize(s);std::memset(samples,0x73,sizeof(samples));}
    void mode(unsigned n){
        p[8]=static_cast<i32>((2*n+1)*(0x7fffffffu/8u)/2u);
    }
    void process(){kiln::process(s,{samples,8192},{1200,block*64*192/44100,true},l,r,p);++block;}
    void noise(){
        for(unsigned i=0;i<64;++i){
            seed=seed*1664525u+1013904223u;l[i]=static_cast<i32>(seed)/4;
            seed=seed*1664525u+1013904223u;r[i]=static_cast<i32>(seed)/4;
        }
    }
    void silence(){std::memset(l,0,sizeof(l));std::memset(r,0,sizeof(r));}
};
void need(bool ok,const char* s){if(!ok){std::puts(s);std::exit(1);}}
}
int main(){
    for(unsigned mode=0;mode<8;++mode){
        Fixture a,b;a.mode(mode);b.mode(mode);
        a.silence();a.process();need(a.s.active==static_cast<int>(mode),"Wrong DSP selected");
        for(unsigned j=0;j<60;++j){a.silence();a.process();for(unsigned i=0;i<64;i++)
            need(!a.l[i]&&!a.r[i],"Stale scratch or noise at rest");}
        bool signal=false;
        for(unsigned j=0;j<190;++j){b.noise();b.process();for(unsigned i=0;i<64;i++)
            if(b.l[i]||b.r[i])signal=true;}
        need(signal,"Silent mode");
        kiln::initialize(b.s);b.silence();b.process();
        for(unsigned i=0;i<64;i++)need(!b.l[i]&&!b.r[i],"Reset retained a tail");
    }
    for(unsigned mode=0;mode<8;++mode){
        Fixture a,b;a.mode(mode);b.mode(mode);b.p[4]=0x7fffffff;
        bool changed=false;
        for(unsigned j=0;j<90;++j){a.noise();b.noise();a.process();b.process();
            if(std::memcmp(a.l,b.l,sizeof(a.l)))changed=true;}
        need(changed==(mode==kiln::Dirt||mode==kiln::Collision||mode==kiln::Spall),"Warmth routing");
    }
    Fixture gates;gates.mode(kiln::Gate);gates.p[1]=0x7fffffff;
    while(kiln::rounded(kiln::mapped(kiln::Gate,0,gates.p[0]),7)!=5)gates.p[0]+=1<<18;
    gates.p[3]=0;gates.p[11]=0x30000000;gates.p[9]=gates.p[6]=0x7fffffff;
    for(unsigned j=0;j<3200;++j){for(unsigned i=0;i<64;i++)gates.l[i]=gates.r[i]=1<<27;
        gates.process();
        if(j>10)for(unsigned i=0;i<64;i++)need(gates.l[i]>(1<<26)&&std::abs(gates.r[i])<8192,"Custom gate banks");}
    // Check makeup on the actual wet path and leave the dry path at unity.
    for(unsigned mode=0;mode<8;++mode){
        Fixture dry;dry.mode(mode);dry.p[5]=0;
        for(unsigned j=0;j<5;++j){dry.noise();i32 expected[64];
            for(unsigned n=0;n<64;++n)expected[n]=(dry.l[n]>>7)*128;
            dry.process();
            if(j)need(!std::memcmp(expected,dry.l,sizeof(expected)),"Output trim changed dry level");
        }
    }
    Fixture loud;loud.mode(kiln::Dirt);
    double inputPower=0,outputPower=0;
    for(unsigned j=0;j<300;++j){loud.noise();
        for(unsigned n=0;n<64;++n){loud.l[n]/=2;loud.r[n]/=2;
            if(j>5){double v=loud.l[n]/2147483648.;inputPower+=v*v;}}
        loud.process();
        if(j>5)for(unsigned n=0;n<64;++n){double v=loud.l[n]/2147483648.;outputPower+=v*v;}
    }
    need(outputPower<3*inputPower && outputPower>.15*inputPower,"Dynamic Dirt level moved outside reference range");
    // A quiet C2 repeatedly excites the least damped root. The wet gain
    // must have enough range to follow that resonance after it settles.
    Fixture tuned;tuned.mode(kiln::Tuned);
    tuned.p[0]=tuned.p[2]=0x7fffffff;tuned.p[1]=tuned.p[3]=0;
    double pluckPower=0,ringPower=0;
    for(unsigned block=0;block<2756;++block){
        for(unsigned n=0;n<64;++n){
            const double t=(block*64+n)/44100.;
            const double v=.012*std::sin(2*3.141592653589793*65.40639*t)*std::exp(-8*std::fmod(t,.375));
            tuned.l[n]=tuned.r[n]=static_cast<i32>(v*2147483648.);
            if(block>=690)pluckPower+=v*v;
        }
        tuned.process();
        if(block>=690)for(unsigned n=0;n<64;++n){
            const double v=tuned.l[n]/2147483648.;
            ringPower+=v*v;
        }
    }
    need(ringPower>.5*pluckPower && ringPower<2*pluckPower,"Tuned Dirt quiet resonant pluck exceeded 3 dB");
    Fixture missing;missing.noise();kiln::process(missing.s,{nullptr,0},{0,0,false},missing.l,missing.r,missing.p);
    for(unsigned i=0;i<64;i++)need(!missing.l[i]&&!missing.r[i],"Missing scratch");
    Fixture stress;
    for(unsigned mode=0;mode<8;mode++)for(unsigned mask=0;mask<16;mask++){
        stress.mode(mode);for(unsigned j=0;j<4;j++)stress.p[j]=(mask&(1u<<j))?0x7fffffff:0;
        for(unsigned k=0;k<24;k++){stress.noise();for(unsigned i=0;i<64;i++){
            stress.l[i]=k%2?(i%2?0x7fffffff:-0x7fffffff-1):stress.l[i]*3;
        }stress.process();}
    }
    std::puts("PASS all eight modes, warm endpoints, custom gate, resets, scratch and extreme controls.");
}
