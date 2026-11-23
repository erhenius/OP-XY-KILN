#pragma once
#include <opxy/effect.hpp>
#include "controls.h"
namespace kiln {
struct Resonator { i32 a,b,g; };
struct ChannelState {
    i32 low,shade,bodyLow,sub,edge,dc,noise,gate,previous,previous2;
    i32 y1[3],y2[3],feedback[2];
};
struct Fragment {
    unsigned start, length, seam, copied, missing, wait, age, life, position;
    i32 speed, amplitude, fadeRate, fallRate, seamScale, tone, low[2];
};
struct SpallState {
    Fragment fragments[2];
    i32 previous[2], difference[2], captureLow[2], fast, slow, inputPeak,fastRem,slowRem;
    unsigned writer, filled, clock, hold, next, quietBlocks;
    bool armed;
};
struct LevelMatch {
    i32 dryMean,wetMean,gain,target;
    i32 dryLow[2],wetLow[2],dryHigh[2],wetHigh[2];
};
struct State {
    ChannelState ch[2];
    i32 fast,slow,history,lastOut[2],mix;
    u32 seed,write,valid,phase,lastTick,playing;
    int active;
    SpallState shards;
    LevelMatch level;
    using ReturnMeterEnabled = void;
};
void initialize(State&);
void process(State&,opxy::Buffer,const opxy::Transport&,opxy::Channel,opxy::Channel,opxy::Parameters);
}
