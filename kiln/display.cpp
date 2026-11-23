#include "display.h"
#include "controls.h"
#include "landscape.h"
namespace kiln {
inline void display_reset(DisplayState& state){state.header.reset();state.revision=0;state.ride=Ride{};}
inline void number(char* s,int n,const char* suffix=""){
    unsigned i=0;if(n<0){s[i++]='-';n=-n;}
    if(n>=1000)s[i++]=static_cast<char>('0'+(n/1000)%10);
    if(n>=100)s[i++]=static_cast<char>('0'+(n/100)%10);
    if(n>=10)s[i++]=static_cast<char>('0'+(n/10)%10);
    s[i++]=static_cast<char>('0'+n%10);
    while(*suffix && i<7)s[i++]=*suffix++;
    s[i]=0;
}
inline void byteText(char* s,unsigned n){
    const char hex[]="0123456789ABCDEF";s[0]=hex[(n>>4)&15];s[1]=hex[n&15];s[2]=0;
}
inline void display(DisplayState& state,const opxy::draw::Input& in,Frame& frame){
    using namespace opxy::draw;
    const unsigned mode=in.choices[0]<8?in.choices[0]:0;
    const bool gate=mode==Gate,warm=mode==Dirt||mode==Collision||mode==Spall;
    const char* labels[8]={titles[mode][0],titles[mode][1],titles[mode][2],titles[mode][3],
                           warm?"warmth":"","mix",gate?"L 9-15":"",gate?"R 9-15":""};
    char values[8][header_capacity]={};
    unsigned v[4];
    for(unsigned i=0;i<4;++i){v[i]=mapped(mode,i,in.slots[i]);number(values[i],rounded(v[i],100),"%");}
    const unsigned pat=rounded(v[0],7);
    const char* sides[]={"Rest","Left","Right","Both"};
    if(gate){
        const char* patterns[]={"Offbeat","Hocket","Broken","Long","C:rest","C:left","C:right","C:both"};
        copy(values[0],header_capacity,patterns[pat]);
        const char* rates[]={"1/8","1/16","1/32"};copy(values[2],header_capacity,rates[rounded(v[2],2)]);
        number(values[3],1+static_cast<int>((48u*(v[3]>>8)*(v[3]>>8)+32768)>>16),"ms");
        for(unsigned i=0;i<2;++i)byteText(values[6+i],choice(in.slots[6+i],128));
    }
    if(mode==Spall){
        number(values[1],(spallGrainUs(v[1])+500)/1000,"ms");
        number(values[2],-static_cast<int>(rounded(squared(v[2]),18)),"st");
        number(values[3],spallSettleMs(v[3]),"ms");
    }
    if(mode==Space)number(values[0],18+rounded(v[0],47),"ms");
    if(mode==Formant){
        const char* vowels[]={"Ah","Eh","Ee","Oh","Oo"};copy(values[0],header_capacity,vowels[rounded(v[0],4)]);
        number(values[1],static_cast<int>(rounded(v[1],24))-12,"st");
    }
    if(mode==Dirt||mode==Collision||mode==Tuned||mode==Memory)number(values[0],rounded(v[0],mode==Memory?24:27),"dB");
    // Interpolation of physical-unit captions stays off the audio path.
    if(mode==Collision || mode==Memory){
        const unsigned splitHz[17]={65,81,100,125,155,192,238,296,368,457,567,704,875,1086,1349,1675,2080};
        const unsigned toneHz[17]={450,554,682,840,1034,1273,1567,1929,2375,2924,3600,4432,5457,6718,8271,10182,12536};
        const unsigned recoveryMs[17]={20,26,33,43,56,72,93,120,155,200,258,333,431,557,719,929,1200};
        for(unsigned col=1;col<4;++col){
            if(mode==Collision && col==2)continue;
            if(mode==Memory && col!=2)continue;
            const unsigned* table=mode==Memory?recoveryMs:col==1?splitHz:toneHz;
            const unsigned k=v[col]>>12,f=v[col]&4095;
            const unsigned amount=table[k]+((table[k+1]-table[k])*f+2048)/4096;
            number(values[col],amount>=10000?amount/1000:amount,mode==Memory?"ms":amount>=10000?"kHz":"Hz");
        }
    }
    if(mode==Dirt)number(values[1],static_cast<int>(rounded(v[1],18))-9,"dB");
    if(mode==Memory)number(values[3],static_cast<int>(rounded(v[3],200))-100,"%");
    if(mode==Tuned){
        const char* note[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        const unsigned midi=36+rounded(v[1],36);
        copy(values[1],header_capacity,note[midi%12]);unsigned n=values[1][1]?2:1;
        values[1][n]=static_cast<char>('0'+midi/12-1);values[1][n+1]=0;
    }
    if(warm)number(values[4],rounded(knob(in.slots[4]),100),"%");
    number(values[5],rounded(knob(in.slots[5]),100),"%");
    if(in.shift){
        for(unsigned i=0;i<8;++i){labels[i]="";values[i][0]=0;}
        labels[0]="mode";copy(values[0],header_capacity,names[mode]);labels[4]=labels[0];
        copy(values[4],header_capacity,values[0]);
        if(gate)for(unsigned i=1;i<3;++i){labels[i]=labels[i+4]=i==1?"L 1-7":"R 1-7";byteText(values[i],in.choices[i]);byteText(values[i+4],in.choices[i]);}
        if(gate){labels[3]=labels[7]="step 8";copy(values[3],header_capacity,sides[in.choices[3]&3]);copy(values[7],header_capacity,values[3]);}
    }
    page_header(state.header,in,frame,labels,values,4);
    // Long unit captions share a 55 px right half with the stock header.
    for(unsigned i=0;i<4;++i){
        auto& value=frame.texts[4+i];
        unsigned len=0;while(value.chars[len])++len;
        if(len>4)value.scale=value.scale*4/len;
    }
    relief::draw(state,in,frame,mode,v,pat);
    text(frame,16,54*unit,210*unit,unit,names[mode]);
}
}
