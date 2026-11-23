#include "../fx.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <cmath>

namespace {
using opxy::i32;
void require(bool ok,const char* why){if(!ok){std::puts(why);std::exit(1);}}
struct Rig {
    kiln::State state;
    i32 memory[8192],l[64],r[64],p[12];
    Rig():p{} {
        kiln::initialize(state);
        std::memset(memory,0x55,sizeof(memory));
        for(unsigned j=0;j<4;++j)p[j]=1<<30;
        p[2]=0;p[3]=0x7fffffff;
    }
    void process(i32 left,i32 right,bool hit=false){
        for(unsigned j=0;j<64;++j){l[j]=left;r[j]=right;}
        if(hit)for(unsigned j=0;j<16;++j)r[j]=(j&1)?(1<<29):-(1<<29);
        kiln::spallBlock(state,{memory,8192},l,r,p,1<<24,false);
    }
};
}
int main(){
    Rig a,b; // Only the left channel has signal in the captured interval.
    for(unsigned i=0;i<140;++i)a.process(1<<26,0);
    a.process(1<<26,0,true);
    require(a.state.shards.hold!=0,"missed right-channel trigger");
    bool heard=false;
    for(unsigned i=0;i<300;++i){
        a.process(0,0);b.process(0,0);
        for(unsigned j=0;j<64;++j){
            heard|=a.l[j]>100000;
            require(!a.r[j],"trigger leaked into its own capture or channels crossed");
            require(!b.l[j]&&!b.r[j],"stale scratch or shared voice");
        }
    }
    require(heard,"no frozen audio after live input ended");
    auto& f=a.state.shards.fragments[a.state.shards.next^1];
    require(f.copied==f.length && f.age<f.life,"snapshot was not frozen");
    require(f.speed==kiln::captureSpeed,"nonzero pitch drift at Fall zero");
    a.process(0,0,true);
    require(a.state.shards.fragments[0].age<a.state.shards.fragments[0].life &&
            a.state.shards.fragments[1].age<a.state.shards.fragments[1].life,"lost overlapping fragment");
    for(unsigned i=0;i<1200;++i)a.process(0,0);
    for(unsigned j=0;j<64;++j)require(!a.l[j]&&!a.r[j],"tail did not end");
    Rig zero;zero.p[0]=0;for(unsigned i=0;i<300;++i)zero.process(1<<25,0,i%80==0);
    require(!zero.state.shards.fragments[0].life&&!zero.state.shards.fragments[1].life,"Fracture zero captured");
    // One quiet snare on a silent send. Check actual output after the hit
    // has ended, with every encoder compared against a common reference.
    for(unsigned control=0;control<5;++control){
        Rig dry,edited;
        dry.p[2]=edited.p[2]=1<<30;
        edited.p[control]=control==4?0x7fffffff:0;
        double tailPower=0,differencePower=0;
        unsigned noise=9;
        for(unsigned block=0;block<420;++block){
            for(unsigned n=0;n<64;++n){
                noise=1664525u*noise+1013904223u;
                const i32 sample=block>=40 && block<46 ? static_cast<i32>(noise)/1024 : 0;
                dry.l[n]=edited.l[n]=sample;
                dry.r[n]=edited.r[n]=sample/2;
            }
            for(Rig* rig : {&dry,&edited})
                kiln::spallBlock(rig->state,{rig->memory,8192},rig->l,rig->r,rig->p,1<<24,false);
            if(block>=46)for(unsigned n=0;n<64;++n){
                const double y=dry.l[n],delta=y-edited.l[n];
                tailPower+=y*y;differencePower+=delta*delta;
            }
        }
        require(tailPower>1e11,"isolated quiet hit has no audible tail");
        require(differencePower>.001*tailPower,"Spall encoder does not change the rendered tail");
    }
    // A mellow repeated note with an 8 ms attack previously produced no grains.
    // Check each onset, tails in each rest, and absence of retriggers while held.
    Rig bass;
    unsigned captures=0;
    double tailEnergy=0;
    for(unsigned block=0;block<2500;++block){
        const unsigned step=block%500;
        if(step==0){captures=0;tailEnergy=0;}
        for(unsigned i=0;i<64;++i){
            const int at=static_cast<int>(step*64+i)-1024;
            const double t=at/44100.;
            const double env=t<0?0:t<.008?t/.008:t<.24?1:t<.252?(.252-t)/.012:0;
            const i32 sample=static_cast<i32>(.18*2147483647.*env*std::sin(691.15038379*t));
            bass.l[i]=bass.r[i]=sample;
        }
        const unsigned previous=bass.state.shards.next;
        kiln::spallBlock(bass.state,{bass.memory,8192},bass.l,bass.r,bass.p,1<<24,false);
        if(previous!=bass.state.shards.next){
            ++captures;require(step>=16 && step<=23,"late bass capture or sustained-note retrigger");
        }
        if(step>=208 && step<340)for(i32 sample:bass.l)tailEnergy+=double(sample)*sample;
        if(step==499)require(captures==1 && tailEnergy>1e13,"missing bass capture or silent tail");
    }
    std::puts("PASS Spall pre-hit stereo capture, freeze across wraps, two tails, Fall zero, clean expiry and send isolation.");
}
