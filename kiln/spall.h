// Included in fx.cpp beside the Q24 arithmetic helpers. State and scratch are per send.
constexpr unsigned historyFrames=2816, fragmentFrames=2688, lookbackFrames=115;
static_assert(historyFrames+2*fragmentFrames==8192,"Spall scratch layout");
constexpr i32 captureSpeed=q(19200.0/44100);
struct SpallControls {
    unsigned length, life;
    i32 amount, triggerRatio, fallRate, fadeRate;
};
inline SpallControls spallControls(opxy::Parameters p){
    const unsigned fracture=mapped(Spall,0,p[0]), fall=mapped(Spall,2,p[2]);
    const i32 amount=norm(fracture);
    const unsigned length=(spallGrainUs(mapped(Spall,1,p[1]))*192u+5000)/10000;
    const unsigned life=(spallSettleMs(mapped(Spall,3,p[3]))*441u+5)/10;
    return {length,life,mul(amount,q(.8)+mul(q(2.65),amount)),q(1.98)-mul(q(.42),amount),
            mul(q(1.03972),norm(squared(fall)))/static_cast<i32>(life),
            q(5.5)/static_cast<i32>(life)};
}
inline unsigned historyIndex(unsigned i){return i>=historyFrames?i-historyFrames:i;}
inline unsigned stereoWord(i32 l,i32 r){
    return (static_cast<u32>(l>>9)&65535u)|(static_cast<u32>(r>>9)<<16);
}
inline i32 channelWord(u32 word,unsigned ch){
    return static_cast<opxy::i16>(word>>(ch*16))*512;
}
inline void capture(SpallState& s,const SpallControls& c,const i32* history,i32 hitPeak){
    Fragment& f=s.fragments[s.next];
    s.next^=1;
    f.start=historyIndex(s.writer+historyFrames-c.length-lookbackFrames);
    f.missing=static_cast<unsigned>(bounded(static_cast<i32>(c.length+lookbackFrames)-static_cast<i32>(s.filled),0,c.length));
    // Prefer the sound before the transient. If the window is effectively
    // empty, take the attack itself. Keep a short pre-roll so even a single
    // hit has something to repeat past the loop crossfade.
    i32 peak=0;
    for(unsigned i=f.missing;i<c.length;i+=1+c.length/48){
        const u32 word=history[historyIndex(f.start+i)];
        for(unsigned ch=0;ch<2;++ch){
            const i32 sample=mag(channelWord(word,ch));
            if(sample>peak)peak=sample;
        }
    }
    f.wait=0;
    if(peak<=bounded(hitPeak>>6,512,unit)){
        const unsigned pre=c.length/3;
        f.start=historyIndex(s.writer+historyFrames-pre);
        f.missing=pre>s.filled?pre-s.filled:0;
        f.wait=c.length-pre;
    }
    f.length=c.length;f.seam=(c.length*21+50)/100;f.seamScale=unit/static_cast<i32>(f.seam);
    f.age=0;f.life=c.life;f.copied=0;f.position=0;f.speed=captureSpeed;
    f.amplitude=c.amount;f.fadeRate=c.fadeRate;f.fallRate=c.fallRate;f.tone=unit;
    f.low[0]=f.low[1]=0;
}
// Freeze one frame per output sample. Before the copier reaches a frame it is
// still in live history: the writer advances only 64 times per 147 output samples.
inline i32 frozen(const i32* history,const i32* slot,const Fragment& f,unsigned i){
    if(i<f.copied)return slot[i];
    return i<f.missing?0:history[historyIndex(f.start+i)];
}
inline void fragmentSample(Fragment& f,i32* history,i32* slot,i32 (&tail)[2]){
    if(f.wait || f.age>=f.life)return;
    if(f.copied<f.length){
        const unsigned i=f.copied;
        slot[i]=frozen(history,slot,f,i);
        ++f.copied;
    }
    const unsigned i=f.position>>16, frac=(f.position&65535u)<<8, period=f.length-f.seam;
    const u32 a=frozen(history,slot,f,i),b=frozen(history,slot,f,i+1);
    u32 endA=0,endB=0;
    i32 seamMix=unit;
    if(i<f.seam){
        // At the last frame wrap interpolation to the equivalent start of the loop.
        endA=frozen(history,slot,f,period+i);
        endB=(period+i+1<f.length)?frozen(history,slot,f,period+i+1):a;
        seamMix=static_cast<i32>(i)*f.seamScale+mul(frac,f.seamScale);
    }
    const i32 gain=f.age<124?mul(f.amplitude,static_cast<i32>(f.age)*(unit/124)):f.amplitude;
    const i32 colour=q(.10)+mul(q(.52),f.tone);
    for(unsigned ch=0;ch<2;++ch){
        i32 sample=blend(channelWord(a,ch),channelWord(b,ch),frac);
        if(i<f.seam)sample=blend(blend(channelWord(endA,ch),channelWord(endB,ch),frac),sample,seamMix);
        tail[ch]+=mul(gain,follow(f.low[ch],sample,colour));
    }
    f.position+=static_cast<unsigned>(f.speed)>>8;
    if(f.position>=period*65536u)f.position-=period*65536u;
    f.speed-=mul(f.speed,f.fallRate);
    f.amplitude-=mul(f.amplitude,f.fadeRate);f.tone-=mul(f.tone,f.fadeRate);
    ++f.age;
}
inline void spallBlock(State& state,opxy::Buffer buffer,opxy::Channel left,opxy::Channel right,
                       opxy::Parameters p,i32 mix,bool changed){
    BlockLevel level(state.level);
    SpallState& s=state.shards;
    const SpallControls c=spallControls(p);
    // A hit is measured against the audio on this send. A slow peak release
    // keeps the threshold steady between attacks. Only the absolute noise floor
    // stays fixed; it suppresses captures from quantization at near-silence.
    i32 blockPeak=0;
    for(unsigned n=0;n<64;++n){
        const i32 l=mag(left[n]>>7),r=mag(right[n]>>7);
        if(l>blockPeak)blockPeak=l;
        if(r>blockPeak)blockPeak=r;
    }
    // Arm an independent onset after ~12 ms of near-silence. Low notes can
    // grow clearly in amplitude while producing too little high-frequency flux.
    // Latch the armed state across the first quiet blocks of a gradual attack.
    const i32 quietFloor=bounded(s.inputPeak>>10,128,unit);
    const i32 wakeFloor=bounded(s.inputPeak>>7,512,unit);
    if(s.quietBlocks<8){
        if(blockPeak<=quietFloor)++s.quietBlocks;
        else s.quietBlocks=0;
    }
    bool wake=s.quietBlocks==8 && blockPeak>=wakeFloor;
    s.inputPeak=bounded(blockPeak,s.inputPeak-(s.inputPeak>>7),unit);
    const i32 warmth=norm(knob(p[4]));
    const i32 margin=32+(s.inputPeak>>7);
    const i32 from[2]={state.lastOut[0],state.lastOut[1]};
    for(unsigned n=0;n<64;++n){
        const i32 x[2]={left[n]>>7,right[n]>>7};
        i32 edge=0;
        for(unsigned ch=0;ch<2;++ch){
            const i32 difference=x[ch]-s.previous[ch];
            const i32 flux=mag(difference-s.difference[ch])+mul(q(.07),mag(difference));
            if(flux>edge)edge=flux;
            s.previous[ch]=x[ch];s.difference[ch]=difference;
            follow(s.captureLow[ch],x[ch],q(.62));
        }
        // Keep sub-LSB envelope updates when the source is quiet.
        const i32 df=edge-s.fast+s.fastRem,ds=edge-s.slow+s.slowRem;
        s.fast+=df>>5;s.slow+=ds>>10;
        s.fastRem=static_cast<u32>(df)&31;s.slowRem=static_cast<u32>(ds)&1023;
        const i32 fast=s.fast,slow=s.slow;
        if(fast<mul(slow,q(1.32))+(margin>>2))s.armed=true;
        if(s.hold)--s.hold;
        const bool newNote=wake && (mag(x[0])>=wakeFloor || mag(x[1])>=wakeFloor);
        if(newNote){s.quietBlocks=0;wake=false;}
        if(!s.hold && (newNote || (s.armed && fast>mul(slow,c.triggerRatio)+margin))){
            if(c.amount)capture(s,c,buffer.words,blockPeak);
            s.hold=3484;s.armed=false;s.quietBlocks=0;wake=false;
        }
        i32 tail[2]={};
        for(unsigned j=0;j<2;++j)
            fragmentSample(s.fragments[j],buffer.words,buffer.words+historyFrames+j*fragmentFrames,tail);
        s.clock+=64;
        if(s.clock>=147){
            s.clock-=147;
            buffer.words[s.writer]=static_cast<i32>(stereoWord(s.captureLow[0],s.captureLow[1]));
            s.writer=historyIndex(s.writer+1);if(s.filled<historyFrames)++s.filled;
            for(unsigned j=0;j<2;++j)if(s.fragments[j].wait)--s.fragments[j].wait;
        }
        const i32 wet=state.mix+static_cast<i32>((opxy::q31::wide(mix-state.mix,n+1))>>6);
        for(unsigned ch=0;ch<2;++ch){
            // Smooth bright repeats while leaving the direct attack intact.
            i32 shard=tail[ch];
            if(warmth){
                const i32 rounded=follow(state.ch[ch].edge,shard,q(.14));
                shard=blend(shard,shape(rounded),warmth);
            }
            const i32 processed=x[ch]+shard;
            const i32 matched=level.apply(state.level,n,ch,x[ch],processed);
            i32 y=blend(x[ch],c.amount?matched:processed,wet);
            if(changed)y=blend(from[ch],y,(n+1)<<18);
            (ch?right:left)[n]=toBus(y);state.lastOut[ch]=bounded(y,-unit,unit-1);
        }
    }
    level.finish(state.level);
    state.mix=mix;
}
