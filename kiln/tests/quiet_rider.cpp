#include "../display.cpp"
#include <cstdio>

// Stock sends can peak far below full scale. Deliver a right-channel meter
// report between animation ticks, then the report for silence before the tick.
static bool reacts(unsigned peak,int elapsed) {
    kiln::DisplayState s={};
    kiln::Frame f={};
    opxy::draw::Input in={};
    in.elapsed=elapsed;
    in.choices[0]=kiln::Spall;
    kiln::display_reset(s);
    bool grabbed=false;
    for(unsigned i=0;i<150;++i){
        s.accept_return_meter(i==2?peak|1u:0);
        kiln::display(s,in,f);
        if(!opxy::draw::valid(f))return false;
        grabbed|=f.nodes[kiln::relief::poseNodes[2]].x!=opxy::draw::hidden_x*opxy::draw::unit;
    }
    return grabbed && !s.ride.energy && !s.ride.power && !s.ride.slow;
}
int main(){
    for(int bit=21;bit<=26;++bit)
        if(!reacts(1u<<bit,16)||!reacts(1u<<bit,21))return 1;
    if(reacts(0,16)||reacts(1u<<15,21))return 2;
    std::puts("Short quiet hits survive the display tick; silence settles");
}
