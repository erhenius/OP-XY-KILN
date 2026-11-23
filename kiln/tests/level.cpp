#include "../fx.cpp"
#include <cstdlib>
#include <cstdio>

int main(){
    const opxy::i32 unit=1<<24;
    kiln::State send;
    kiln::initialize(send);
    for(int stage=0;stage<4;++stage){
        const opxy::i32 expected[]={unit/4,2*unit,unit,unit};
        const int wetScale[]={4,1,2,0};
        const int dryScale[]={1,2,2,0};
        for(unsigned b=0;b<2200;++b){
            kiln::BlockLevel meter(send.level);
            for(unsigned n=0;n<64;++n){
                const opxy::i32 sample=((n&8)?1:-1)*(unit/16);
                const opxy::i32 l=meter.apply(send.level,n,0,sample*dryScale[stage],sample*wetScale[stage]);
                const opxy::i32 r=meter.apply(send.level,n,1,-sample*dryScale[stage],-sample*wetScale[stage]);
                if(std::abs(l+r)>1)return 1;
            }
            meter.finish(send.level);
        }
        if(std::abs(send.level.gain-expected[stage])>unit/50)return 2;
    }
    std::puts("PASS wet level matching, stereo linkage, silence and recovery.");
}
