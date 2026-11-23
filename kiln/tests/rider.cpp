#include "../display.cpp"
#include <cstdio>

int main() {
    kiln::DisplayState state={};
    kiln::Frame frame={};
    opxy::draw::Input input={};
    kiln::display_reset(state);
    input.choices[0]=kiln::Spall;
    input.elapsed=51;
    for(auto& slot:input.slots)slot=0x40000000;
    kiln::display(state,input,frame);
    const auto rest=frame.nodes[0].y;
    const auto riderX=frame.nodes[kiln::relief::poseNodes[0]].x;
    const auto geometry=frame.paths[0].key;
    int lastCut=frame.nodes[96].x;
    bool travelled=false,grabbed=false;
    for(unsigned tick=0;tick<70;++tick) {
        state.accept_return_meter(tick<3?0x70000000u+tick:0);
        kiln::display(state,input,frame);
        if(!opxy::draw::valid(frame)||frame.paths[0].key!=geometry)return 1;
        if(!state.ride.power)continue;
        const auto cut=frame.nodes[96].x;
        if(cut>lastCut)return 2;
        travelled|=cut<lastCut;lastCut=cut;
        const auto& grab=frame.nodes[kiln::relief::poseNodes[2]];
        if(grab.x!=opxy::draw::hidden_x*opxy::draw::unit) {
            if(grab.x!=riderX)return 3;
            grabbed=true;
        }
    }
    if(!travelled||!grabbed||frame.nodes[0].y!=rest)return 4;
    std::puts("Spall crosses a left-moving fracture, lands and settles");
}
