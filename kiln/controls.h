#pragma once
#include <opxy/types.hpp>

namespace kiln {
using opxy::i32; using opxy::u32;
enum Mode { Collision, Space, Formant, Gate, Dirt, Memory, Tuned, Spall };
const char* const names[]={"Collision","Transient Space","Formant","Stereo Gate","Dynamic Dirt","Memory Drive","Tuned Dirt","Spall"};
const char* const titles[][4]={
    {"Drive","Split","Impact","Colour"},{"Size","Focus","Width","Damping"},
    {"Vowel","Shift","Resonance","Motion"},{"Pattern","Depth","Rate","Edge"},
    {"Drive","Tilt","Focus","Wear"},{"Drive","Memory","Recovery","Bias"},
    {"Drive","Root","Ring","Damping"},{"Fracture","Grain","Fall","Settle"}};
// Mid travel recalls the sound of the audition. Other positions still span the full control.
const unsigned centres[][4]={{62,36,71,58},{56,28,83,44},{21,50,49,35},{14,100,50,25},{57,49,68,18},{46,71,49,73},{52,30,70,37},{74,55,28,73}};
inline unsigned knob(i32 word){return word<0?0:static_cast<u32>(word)>>15;}
inline unsigned mapped(unsigned mode,unsigned col,i32 word){
    const unsigned v=knob(word),c=(centres[mode][col]*65535+50)/100;
    if(v<32768)return (v*c)>>15;
    return c+(((v-32768)*(65535-c)+16383)/32767);
}
inline unsigned choice(i32 word,unsigned count){return (knob(word)*count)>>16;}
inline unsigned rounded(unsigned v,unsigned max){return (v*max+32767)/65535;}
// Work in Q16 so the display and the audio path use the same dial curves.
inline unsigned squared(unsigned v){return (v*v)>>16;}
inline unsigned spallGrainUs(unsigned v){return 18000+115*((squared(v)*1000u+32768)>>16);}
inline unsigned spallSettleMs(unsigned v){return 75+((1480*squared(v)+32768)>>16);}
const unsigned patterns[4][2]={{0x3333,0x3333},{0xe2e2,0x1d1d},{0xcac6,0x98ac},{0xfcf8,0xf8fc}};
inline unsigned gateMask(unsigned pattern,unsigned ch,unsigned first,unsigned last,unsigned step8,unsigned step16){
    return pattern<4?patterns[pattern][ch]:((first<<9)|(((step8>>ch)&1)<<8)|(last<<1)|((step16>>ch)&1));
}
}
