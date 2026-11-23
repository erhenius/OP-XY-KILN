// Stereo-linked gain riding. Meter 16 evenly spaced frames per channel per
// block, before wet/dry mixing. Apply the same smooth gain to both channels.
struct BlockLevel {
    i32 dry=0,wet=0,gain,step;
    explicit BlockLevel(const LevelMatch& state)
        :gain(state.gain),step((state.target-state.gain)/64){}
    static i32 weight(i32 x,i32& low,i32& high){
        x=bounded(x,-8*unit,8*unit)>>4;
        // At the meter's 11.025 kHz rate, de-emphasize sub-bass and give
        // added high harmonics extra weight. The audio itself is unfiltered.
        low+=(x-low)>>5;
        high+=(x-high)>>1;
        return mag(x-low+(low>>2)+((x-high)>>1));
    }
    inline i32 apply(LevelMatch& state,unsigned n,unsigned ch,i32 x,i32 y){
        if(!(n&3)){
            dry+=weight(x,state.dryLow[ch],state.dryHigh[ch]);
            wet+=weight(y,state.wetLow[ch],state.wetHigh[ch]);
        }
        if(!ch)gain+=step;
        return mul(y,gain);
    }
    void finish(LevelMatch& state, i32 minimumRatio=256){
        state.gain=state.target;
        // ~185 ms measurement window. Both sums have the same scale.
        state.dryMean+=(dry-state.dryMean)>>7;
        state.wetMean+=(wet-state.wetMean)>>7;
        // Freeze on rests/decay; never chase a silent return or amplify hiss.
        if(dry<1024 || wet<512 || state.dryMean<1024 || state.wetMean<512)return;
        const i32 denominator=1+(state.wetMean>>7);
        const i32 numerator=state.dryMean>>7;
        // Bound the dividend before shifting; one 32-bit division per block.
        const i32 ratio=(bounded(numerator,0,2*denominator)*1024)/denominator;
        const i32 wanted=bounded(ratio,minimumRatio,2048)*(unit/1024);
        const i32 difference=wanted-state.gain;
        state.target=state.gain+(difference>>(difference<0?6:8));
    }
};
