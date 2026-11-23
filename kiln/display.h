#pragma once
#include <opxy/page_header.hpp>
namespace kiln {
using Frame=opxy::draw::Frame<4,64,121,17,4>;
struct Ride {
    opxy::u32 word, previous, pending;
    int ticks, stale, energy, slow, age, span, power, cooldown, lift[3];
    bool primed;
};
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::u32 last[3], revision;
    Ride ride;
    void accept_return_meter(opxy::u32 level) { ride.word = level; }
};
}
