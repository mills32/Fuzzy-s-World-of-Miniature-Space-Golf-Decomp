#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <dirent.h>
#include <sys/stat.h>
#undef outp

/* macro to write a word to a port */
#define word_out(port,register,value) \
  outport(port,(((word)value<<8) + register))

#define true 1
#define false 0

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;


//ADLIB/SBlaster = 388
//SB16+ = 220 or 240
//opl2lpt = 378, 379 y 37A
int ADLIB_PORT = 0;//0x388;
void Clearkb(){
	asm mov ah,00ch
	asm mov al,0
	asm int 21h
}
void wait(){
    int i = 180;
    while(i--){
        asm mov		dx,0x03DA
        WaitNotVsync: asm in al,dx; asm test al,08h;asm jnz WaitNotVsync;
        WaitVsync: asm in al,dx; asm test al,08h; asm jz WaitVsync;
    }
}

void Init();
void Exit_Dos();
extern unsigned char C_Volume[];
void Wait_Vsync(){
    asm mov		dx,0x03DA
    WaitNotVsync: asm in al,dx; asm test al,08h;asm jnz WaitNotVsync;
    WaitVsync: asm in al,dx; asm test al,08h; asm jz WaitVsync;
}
byte CldsPlayer_load(char *filename);
void CldsPlayer_get_songlength(void);
void CldsPlayer_update(void);


byte NUM_INSTRUMENTS = 32;
byte PATTERN_ROWS = 64;
byte NUM_PATT = 0;
word XM_patdata = 0;
byte *XM_buf;
int XM_row = 0;
int XM_pat = 0;
byte playing = 0;
byte XM_Ins_arp_tab[12] = {0};
byte XM_Ins_car_misc = 0;
byte XM_Ins_car_vol = 0;
byte XM_Ins_A = 0; byte XM_Ins_D = 0;
byte XM_Ins_S = 0; byte XM_Ins_R = 0;
word XM_Ins_atk_ticks = 0; word XM_Ins_dec_ticks = 0;
word XM_Ins_rel_ticks = 0; word XM_Ins_sus_vol = 0;
byte XM_Ins_trem_speed = 0;
byte XM_Ins_trem_rate = 0;
byte XM_Ins_tremwait = 0;
byte XM_Ins_keyoff = 0;
byte XM_Ins_portamento = 0;
byte XM_Ins_glide = 0;
byte XM_Ins_finetune = 0;
byte XM_Ins_vibrato_rate = 0;
byte XM_Ins_vibrato_depth = 0;
byte XM_Ins_vibdelay = 0;
byte XM_Ins_arpeggio = 0;
byte XM_Ins_fadeout = 0;
byte XM_Ins_MIDI = 0;

FILE *f;//XM file
int *lds_DIV1216;
int *lds_MOD1216;
unsigned short lds_frequency[];
void lds_get_instrument(int inst_number);

void lds_test_all_instruments();


typedef struct {
    char id[4];          // "IMPM"
    char song_name[26];  // Song name (null-padded)
    word highlight,ordnum,insnum,smpnum;
    word patnum,cwtv,cmwt,flags,special;
    byte globalvol,mixvol,initialspeed;
    byte initialtempo,pansep,pwd;
    word message_length;
    unsigned long message_offset,reserved;
    byte ch_pan[64];
    byte ch_vol[64];
} ITHeader;

typedef struct {
    char id[4];    // "IMPI"
    char filename[12];
    byte zero,nna,dct,dca;
    word fadeout;
    char pps;
    byte ppc,gbv,dfp,rv,rp;
    word trkvers;
    byte nos,reserved1;
    char name[26];
    byte ifc,ifr,mch,mpr;
    word midibnk;
    byte keymap[240];
    // Volume envelope
    byte vol_flg,vol_pts,vol_loop_beg;
    byte vol_loop_end,vol_sus_beg,vol_sus_end;
    byte vol_nodes[75];//75 pairs: byte y (vol 0 64) + word x (tick number 0-9999)
    byte res0;
    // Pan envelope (unused)
    byte pan_flg,pan_pts,pan_loop_beg;
    byte pan_loop_end,pan_sus_beg,pan_sus_end;
    byte pan_nodes[75];//75 pairs: byte y (pan -32 +32) + word x (tick number 0-9999)
    byte res2;
    // Pitch envelope
    byte pit_flg,pit_pts,pit_loop_beg;
    byte pit_loop_end,pit_sus_beg,pit_sus_end;
    byte pit_nodes[75];//75 pairs: byte y (pitch -32 +32) + word x (tick number 0-9999)
    byte res1;
    unsigned long pad;
} ITInstrument;

typedef struct {
    char  id[4];    // "IMPS"
    char  filename[12];
    byte  zero,gbv,flg,vol;
    char  name[26];
    byte  cvt,dfp;
    unsigned long length,loop_beg,loop_end,C5speed;
    unsigned long sloop_beg,sloop_end,sample_pointer;
    byte  vir,vid,vis,vit;
    byte cache[16];
} IMPsample;

unsigned long patt_offsets[128] = {0};
long patt_table_pos = 0;

#include <math.h>

int opl2_to_it_volume(int tl){
    float attenuation_db,amp;
    int it_vol;

    if(tl < 0) tl = 0;
    if(tl > 63) tl = 63;

    attenuation_db = tl * 0.75;
    amp = pow(10.0, -attenuation_db / 20.0);

    it_vol = (int)(64.0 * amp + 0.5);

    if(it_vol < 0) it_vol = 0;
    if(it_vol > 64) it_vol = 64;

    return it_vol;
}

int write_IT_header(byte numinst) {
    int i = 0;int k = 0;int j = 0;
    unsigned long inst_offsets[128] = {0};
    unsigned long samp_offsets[128] = {0};
    long inst_table_pos = 0;
    long samp_table_pos = 0;
    unsigned long patt_pos = 0;
    word *envelope_ticks;
    ITHeader header;

    memset(&header, 0, sizeof(header));
    memcpy(header.id, "IMPM", 4);
    strncpy(header.song_name,"Empty Module", 25);

    header.ordnum = 128; header.patnum = 128;
    header.smpnum = numinst; header.insnum = numinst;
    header.flags = 0x00C5;
    header.cwtv = 0x0214;  // IT 2.14
    header.cmwt = 0x0200;
    header.globalvol = 128; header.mixvol = 128;
    header.initialspeed = 6; header.initialtempo = 175;//70Hz like original LDS
    header.pansep = 128;

    // Default channel settings
    for (i = 0; i < 64; i++) {
        header.ch_pan[i] = 32;  // center
        header.ch_vol[i] = 64;  // full volume
    }
    fwrite(&header, sizeof(header), 1, f);

    //Orders (Length = OrdNum)
    for (i = 0; i < header.ordnum; i++) fputc(i,f);
    // Instrument pointer table
    inst_table_pos = ftell(f);
    fwrite(inst_offsets, 4,header.insnum, f);
    // Samples pointer table
    samp_table_pos = ftell(f);
    fwrite(samp_offsets, 4,header.smpnum, f);
    // Pattern pointer table
    patt_table_pos = ftell(f);
    fwrite(patt_offsets, 4,header.patnum, f);
    // Instruments
    for (i = 0; i < header.insnum; i++) {
        ITInstrument ins;
        int x = 0; int y = 0;
        lds_get_instrument(i);//Get LDS instrument settings
        inst_offsets[i] = ftell(f);
        memset(&ins, 0, sizeof(ins));
        memcpy(ins.id, "IMPI", 4);
        sprintf(ins.name,"Instrument %d", i + 1);
        ins.fadeout = XM_Ins_R; ins.trkvers = 0x5132;
        ins.gbv = 128; ins.dfp = 128; ins.nos = 0x01;
        ins.mpr = 0xFF; ins.midibnk = 0xFFFF;
        k = 0;
        // Keymap:
        while (k != 240){ins.keymap[k] = (k>>1); ins.keymap[k+1] = i+1;k+=2;}
        //Convert carrier envelope to IT
        XM_Ins_atk_ticks = XM_Ins_A >> 1;
        XM_Ins_dec_ticks = XM_Ins_D >> 1;
        XM_Ins_sus_vol = (XM_Ins_S<<2)&0xFF;
        //if (XM_Ins_atk_ticks == 15) XM_Ins_sus_vol = 0;//"infinite" attack
        //if (XM_Ins_dec_ticks == 16) XM_Ins_sus_vol = 64;//"infinite" decay
        XM_Ins_rel_ticks = XM_Ins_R;
    // Volume envelope: enabled (ADSR) translates vwet well
        ins.vol_flg = 0x13; //on + loop + autorelease
        ins.vol_pts = 6;
        ins.vol_loop_beg = 2; ins.vol_loop_end = 4;
        //point 0
        if (XM_Ins_atk_ticks == 0) y = 64;
        ins.vol_nodes[0] = y;
        envelope_ticks = (word*) &ins.vol_nodes[1]; envelope_ticks[0] = x;
        //point 1 attack
        if (XM_Ins_atk_ticks == 15) y = 0;
        x += XM_Ins_atk_ticks; y = 64;
        ins.vol_nodes[3] = y;
        envelope_ticks = (word*) &ins.vol_nodes[4]; envelope_ticks[0] = x;
        //point 2 decay
        x += XM_Ins_dec_ticks; y = XM_Ins_sus_vol;
        ins.vol_nodes[6] = y;
        envelope_ticks = (word*) &ins.vol_nodes[7]; envelope_ticks[0] = x;
        //point 3 simulate LDStremolo
        //XM_Ins_trem_speed, XM_Ins_trem_rate (depth), XM_Ins_tremwait
        x += (15 - XM_Ins_trem_speed)<<1; y = XM_Ins_sus_vol + (XM_Ins_trem_rate<<1);
        ins.vol_nodes[9] = y;
        envelope_ticks = (word*) &ins.vol_nodes[10]; envelope_ticks[0] = x;
        //point 4 end of sustain
        x += (15 - XM_Ins_trem_speed)<<1; y = XM_Ins_sus_vol;
        ins.vol_nodes[12] = y;
        envelope_ticks = (word*) &ins.vol_nodes[13]; envelope_ticks[0] = x;
        //point 5
        x += XM_Ins_rel_ticks; y = 0;
        if (XM_Ins_rel_ticks == 15) y = XM_Ins_sus_vol; //"infinite" release
        ins.vol_nodes[15] = y;
        envelope_ticks = (word*) &ins.vol_nodes[16]; envelope_ticks[0] = x;
    // Pan envelope: disabled
        ins.pan_flg = 0x00; //on + loop on XM_Ins_arp_tab[0]
        ins.pan_loop_end = 0x02;
    // Pitch envelope: enabled
        if (XM_Ins_arpeggio){
            byte size = XM_Ins_arpeggio & 15;
            byte speed = XM_Ins_arpeggio >>4;
            x = 0;
            ins.pit_flg = 0x03; //on + loop on
            ins.pit_pts = size<<1;
            ins.pit_loop_beg = 0; ins.pit_loop_end = (size<<1)-1;
            j=0;
            for (k = 0; k != size; k++){
                ins.pit_nodes[j] = XM_Ins_arp_tab[k]<<1; j++;
                envelope_ticks = (word*) &ins.pit_nodes[j]; envelope_ticks[0] = x; j+=2;
                if (k == size-1) speed++; x+=speed;
                ins.pit_nodes[j] = XM_Ins_arp_tab[k]<<1; j++;
                envelope_ticks = (word*) &ins.pit_nodes[j]; envelope_ticks[0] = x; j+=2;
            }
        } else ins.pit_flg = 0x00; //off

        fwrite(&ins, sizeof(ins), 1, f);
    }
    // Samples (empty)
    for (i = 0; i < header.insnum; i++) {
        IMPsample smp;
        lds_get_instrument(i);//Get LDS instrument settings for instrument vibrato
        samp_offsets[i] = ftell(f);
        memset(&smp, 0, sizeof(smp));
        memcpy(smp.id, "IMPS", 4);
        sprintf(smp.name,"Sample %d", i + 1);
        smp.gbv = 128; smp.dfp = 0;
        smp.vir = XM_Ins_vibrato_rate<<1; //Rate
        smp.vid = XM_Ins_vibrato_depth<<1; //depth
        smp.vis = XM_Ins_vibdelay*6; //Sweep-delay
        smp.vit = 0;  //wave (sine)
        fwrite(&smp, sizeof(smp), 1, f);
    }
    patt_pos = ftell(f);
    // Write back instrument offsets
    fseek(f, inst_table_pos, SEEK_SET);
    fwrite(inst_offsets,4,numinst, f);

    // Write back empty sample offsets
    fseek(f, samp_table_pos, SEEK_SET);
    fwrite(samp_offsets,4,numinst, f);

    //Go back to pattern writting
    fseek(f, patt_pos, SEEK_SET);
    return 0;
}

void write_IT_event(byte chan, byte end_row, byte note, byte ins, byte vol, byte eff, byte eff_set){
    //1 byte => 80 (new data) + 0X (X chanel); 0x00 (end of row);
    //1 byte => Bit 0 note present, 1 instrument, 2 Vol/pan; 3 command;
    //if note: 1 byte => 0xFE (Note Cut); 0xFF (Note Off); 0x00 (empty); 1-120 (any note);
    //if instrument: 1 byte => 0x00 (no instrument); 1-99 (any instrument);
    //if volume: 1 byte => 0–64 (Volume); 65–74	(F vol up); 75–84 (F vol down);
    //          85–94 (Vol slide up); 95–104 (Vol slide down); 128–192 (Panning);
    //                    0x02   +  order  jump

    byte IT_Pack[7] = {0x80,0x00,0x00,0x00,0x00,0x00,0x00};
    byte bytes = 2;//
    if (end_row){
        XM_buf[XM_patdata] = 0x00;
        XM_patdata++;
        bytes = 1;
    } else {
        IT_Pack[0] |= chan+1;
        //Write position
        if (note == 0xFF){
            IT_Pack[1] &= 0x00; //disable all
        } else {
            if (note == 0xFC) note = 0xFE;//cut
            else if (note == 0xFD) note = 0xFF;//off
            else note += 12;
            IT_Pack[1] |= 0x01; //enable note
            IT_Pack[bytes++] = note; // NOTE
        }
        if (ins != 0xFF){
            IT_Pack[1] |= 0x02;
            IT_Pack[bytes++] = ins +  1; // instrument
        }
        if (vol){
            IT_Pack[1] |= 0x04;
            IT_Pack[bytes++] = opl2_to_it_volume(64-vol); // volume //DONE
        }
        if (eff){//1 tenpo  2 jump  7 porta  8 vibra  a arp
            IT_Pack[1] |= 0x08;
            if (eff == 0x01){//tempo //DONE
                IT_Pack[bytes++] = eff; IT_Pack[bytes++] = eff_set+1; //
            }
            if (eff == 0x02){//jump to //DONE
                IT_Pack[bytes++] = eff; IT_Pack[bytes++] = eff_set; // jump to
            }
            if (eff == 0x07){//glide to //DONE
                // specify target note in "note";
                IT_Pack[bytes++] = eff; IT_Pack[bytes++] = eff_set;//porta_speed
            }
            if (eff == 0x08){//vibrato //DONE
                IT_Pack[bytes++] = eff; IT_Pack[bytes++] = eff_set>>1;//(speed<<4) | depth;
            }

            if (eff == 0x0A){//arpeggio
                IT_Pack[bytes++] = eff; IT_Pack[bytes++] = eff_set; //0xAB = +note +note
            }
        }
        memcpy(&XM_buf[XM_patdata],IT_Pack,bytes);
        XM_patdata+=bytes;
    }
}

void write_IT_pattern(word rows,byte pattern_number){
    //Write pattern when LDS patterns ends
    patt_offsets[pattern_number] = ftell(f);
    fwrite(&XM_patdata, 2, 1, f);//length
    fwrite(&rows, 2, 1, f);
    fputc(0,f);fputc(0,f);fputc(0,f);fputc(0,f);
    fwrite(XM_buf, 1, XM_patdata, f);//pattern data
    XM_patdata = 0;
}

int main(int argc, char **argv){
    if (argc != 2){ printf("USE: lds2xm music.lds"); wait(); Exit_Dos();}
    ADLIB_PORT = 0x0388;
    Init();
    CldsPlayer_load(argv[1]);playing = 1;
    lds_test_all_instruments();
    playing = 1;
    //create IT
    f = fopen("output.it", "wb");
    if (!f) { printf("file error"); wait(); fclose(f); Exit_Dos();}
    write_IT_header(NUM_INSTRUMENTS);
    while(playing){CldsPlayer_update();Wait_Vsync();if (kbhit()) playing = 0;}
    NUM_PATT = XM_pat;
    //if(!NUM_PATT) NUM_PATT = 1;
    fseek(f,patt_table_pos, SEEK_SET);
    fwrite(patt_offsets,4,NUM_PATT, f);
    fclose(f);

    Clearkb();
    printf("IT file created!");
    wait();
    Exit_Dos();

    return 0;
}


//////////////////

// @iport - index port, @iport + 1 - data port

#define word_out(port,register,value) \
  outport(port,(((word)value<<8) + register))


/* macro to write a word to a port */
#define word_out(port,register,value) \
  outport(port,(((word)value<<8) + register))

#define true 1
#define false 0

#define JUMPMARKER	0x80	// MODULES Orderlist jump marker

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;

void CldsPlayer_update (void);
extern int ADLIB_PORT;
	
	//////////
	//TABLES//
	//////////
int ram_size = 0;//
unsigned char huge *ADPLUG_music_data;
const unsigned short CPlayer_note_table[12] =
  {363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647, 686};
const unsigned char CPlayer_op_table[9] =
  {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};
//Channels volume (if percusion mode, then 6 FM + 5 drums)
unsigned char percussion_mode = 0;
unsigned char C_Volume[11] = {0,0,0,0,0,0,0,0,0,0,0};
unsigned char  Key_Hit[11] = {0,0,0,0,0,0,0,0,0,0,0};

	///////////
	//STRUCTS//
	///////////

typedef struct{
    unsigned char note,command,inst,param2,param1;
} mod_tracks;

//LOUDNESS

typedef struct {
	unsigned char	mod_misc, mod_vol, mod_ad, mod_sr, mod_wave,
    car_misc, car_vol, car_ad, car_sr, car_wave, feedback, keyoff,
    portamento, glide, finetune, vibrato, vibdelay, mod_trem, car_trem,
    tremwait, arpeggio, arp_tab[12];
	unsigned short	start, size;
	unsigned char	fms;
	unsigned short	transp;
	unsigned char	midinst, midvelo, midkey, midtrans, middum1, middum2;
} LDS_SoundBank;

typedef struct {
	unsigned short	gototune, lasttune, packpos;
	unsigned char	finetune, glideto, portspeed, nextvol, volmod, volcar,
    vibwait, vibspeed, vibrate, trmstay, trmwait, trmspeed, trmrate, trmcount,
    trcwait, trcspeed, trcrate, trccount, arp_size, arp_speed, keycount,
    vibcount, arp_pos, arp_count, packwait, arp_tab[12];

	struct {
		unsigned char	chandelay, sound;
		unsigned short	high;
	} chancheat;
} LDS_Channel;

typedef struct {
	unsigned short	patnum;
	unsigned char	transpose;
} LDS_Position;


	/////////////
	//VARIABLES//
	/////////////

unsigned short tempo, bpm, nop;
mod_tracks *tracks[64*9];
byte voiceKeyOn[11];
byte notePitch[11];
int	halfToneOffset[11];
word *fNumFreqPtr[11];
byte noteDIV12[256]; //Store note/12
byte noteMOD12[256];

// Note frequency table (16 notes / octave)
unsigned short lds_frequency[] = {
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
  343, 344, 345, 347, 348, 349, 350, 352, 353, 354, 356, 357, 358,
  359, 361, 362, 363, 365, 366, 367, 369, 370, 371, 373, 374, 375,
  377, 378, 379, 381, 382, 384, 385, 386, 388, 389, 391, 392, 393,
  395, 396, 398, 399, 401, 402, 403, 405, 406, 408, 409, 411, 412,
  414, 415, 417, 418, 420, 421, 423, 424, 426, 427, 429, 430, 432,
  434, 435, 437, 438, 440, 442, 443, 445, 446, 448, 450, 451, 453,
  454, 456, 458, 459, 461, 463, 464, 466, 468, 469, 471, 473, 475,
  476, 478, 480, 481, 483, 485, 487, 488, 490, 492, 494, 496, 497,
  499, 501, 503, 505, 506, 508, 510, 512, 514, 516, 518, 519, 521,
  523, 525, 527, 529, 531, 533, 535, 537, 538, 540, 542, 544, 546,
  548, 550, 552, 554, 556, 558, 560, 562, 564, 566, 568, 571, 573,
  575, 577, 579, 581, 583, 585, 587, 589, 591, 594, 596, 598, 600,
  602, 604, 607, 609, 611, 613, 615, 618, 620, 622, 624, 627, 629,
  631, 633, 636, 638, 640, 643, 645, 647, 650, 652, 654, 657, 659,
  662, 664, 666, 669, 671, 674, 676, 678, 681, 683,
};

// Vibrato (sine) table
unsigned char lds_vibtab[] = {
  0, 13, 25, 37, 50, 62, 74, 86, 98, 109, 120, 131, 142, 152, 162,
  171, 180, 189, 197, 205, 212, 219, 225, 231, 236, 240, 244, 247,
  250, 252, 254, 255, 255, 255, 254, 252, 250, 247, 244, 240, 236,
  231, 225, 219, 212, 205, 197, 189, 180, 171, 162, 152, 142, 131,
  120, 109, 98, 86, 74, 62, 50, 37, 25, 13
};

// Tremolo (sine * sine) table
unsigned char lds_tremtab[] = {
  0, 0, 1, 1, 2, 4, 5, 7, 10, 12, 15, 18, 21, 25, 29, 33, 37, 42, 47,
  52, 57, 62, 67, 73, 79, 85, 90, 97, 103, 109, 115, 121, 128, 134,
  140, 146, 152, 158, 165, 170, 176, 182, 188, 193, 198, 203, 208,
  213, 218, 222, 226, 230, 234, 237, 240, 243, 245, 248, 250, 251,
  253, 254, 254, 255, 255, 255, 254, 254, 253, 251, 250, 248, 245,
  243, 240, 237, 234, 230, 226, 222, 218, 213, 208, 203, 198, 193,
  188, 182, 176, 170, 165, 158, 152, 146, 140, 134, 127, 121, 115,
  109, 103, 97, 90, 85, 79, 73, 67, 62, 57, 52, 47, 42, 37, 33, 29,
  25, 21, 18, 15, 12, 10, 7, 5, 4, 2, 1, 1, 0
};

// 'maxsound' is maximum number of patches (instruments)
// 'maxpos' is maximum number of entries in position list (orderlist)
unsigned short  lds_maxsound = 0x3f,  lds_maxpos = 0xff;
LDS_SoundBank	lds_soundbank[0x3f];
LDS_Channel	*lds_channel;
LDS_Position *lds_positions;
unsigned char fmchip[0xff], jumping, fadeonoff, allvolume, hardfade,
  tempo_now, pattplay, louness_tempo, regbd, chandelay[9], mode, pattlen;
unsigned short	posplay, jumppos, *lds_patterns, lds_speed;
byte songlooped;
unsigned int numpatch, numposi, patterns_size, mainvolume;
int *lds_DIV1216;
int *lds_MOD1216;
	////////////
	//FUNCTIOS//
	//////////// 

//OPL2
void opl2_write(unsigned char reg, unsigned char data){
	asm mov ah,0
	asm mov dx, ADLIB_PORT
	asm mov al, reg
	asm out dx, al
	
	//Wait at least 3.3 microseconds
	asm mov cx,16
	wait:
		asm in ax,dx
		asm loop wait	//for (i = 0; i < 6; i++) inp(lpt_ctrl);
	
	asm inc dx
	asm mov al, data
	asm out dx, al
	
	// Wait at least 23 microseconds
	asm mov cx,23
	wait2:
		asm in ax,dx
		asm loop wait2//for (i = 0; i < 35; i++) inp(lpt_ctrl);
}
void opl2_clear(void){
	int i, slot1, slot2;
    static unsigned char slotVoice[9][2] = {{0,3},{1,4},{2,5},{6,9},{7,10},{8,11},{12,15},{13,16},{14,17}};
    static unsigned char offsetSlot[18] = {0,1,2,3,4,5,8,9,10,11,12,13,16,17,18,19,20,21};
    
    opl2_write(   1, 0x20);   // Set WSE=1
    opl2_write(   8,    0);   // Set CSM=0 & SEL=0
    opl2_write(0xBD,    0);   // Set AM Depth, VIB depth & Rhythm = 0
    
    for(i=0; i<9; i++)
    {
        slot1 = offsetSlot[slotVoice[i][0]];
        slot2 = offsetSlot[slotVoice[i][1]];
        
        opl2_write(0xB0+i, 0);    //turn note off
        opl2_write(0xA0+i, 0);    //clear frequency

        opl2_write(0xE0+slot1, 0);
        opl2_write(0xE0+slot2, 0);

        opl2_write(0x60+slot1, 0xff);
        opl2_write(0x60+slot2, 0xff);
        opl2_write(0x80+slot1, 0xff);
        opl2_write(0x80+slot2, 0xff);

        opl2_write(0x40+slot1, 0xff);
        opl2_write(0x40+slot2, 0xff);
    }
	for (i = 0; i < 6; i++) inp(0x0388);
    for (i=1; i< 256; opl2_write(i++, 0));    //clear all registers
}
void Init(){
    int i,j;
    word RAM = 0;
    for (i = 0; i < 256; i++) noteDIV12[i] = i/12;
    for (i = 0; i < 256; i++) noteMOD12[i] = i%12;
    //allocate main data
    ADPLUG_music_data  = calloc(65535,1);
    lds_DIV1216 = (int*) calloc(16*1024,1);
    lds_MOD1216 = (int*) calloc(16*1024,1);
    //arrange pointers
    lds_positions = (LDS_Position*) ADPLUG_music_data;
    lds_channel = (LDS_Channel*) (ADPLUG_music_data + (1024*sizeof(LDS_Position)));
    for (i = 0; i < 64*9; i++) {
        tracks[i] = (mod_tracks*) calloc(64*5,1);
        RAM+=(64*5);
    }
    //precalculate lds value/(12 * 16) - 1
    for (i = 0; i < 2048; i++) lds_DIV1216[i] = i/(12 * 16) - 1;
    for (i = 0; i < 2048; i++) lds_MOD1216[i] = i%(12 * 16);
    XM_buf = calloc(1024*16,1);
}
void Exit_Dos(){
	int i;
	opl2_clear();
    free(lds_DIV1216);
    free(lds_MOD1216);
    free(ADPLUG_music_data );
	ADPLUG_music_data = NULL;
	//Reset text mode
	asm mov ax, 3h
	asm int 10h
	exit(1);
}

//LOUDNESS PLAYER
byte CldsPlayer_load(char *filename){
    word	i, j;
    LDS_SoundBank	*sb;
    FILE *f = fopen(filename,"rb");
    if(!f) {
        printf("error open");
        return false;
    }
    //return false;
    lds_patterns = (unsigned short*) (ADPLUG_music_data + (1024*sizeof(LDS_Position)) + (9*sizeof(LDS_Channel)));

    //memset(ADPLUG_music_data,0,0xFFFF);

    opl2_clear();
    // init all with 0
    tempo_now = 3; playing = true; songlooped = false;
    jumping = fadeonoff = allvolume = hardfade = pattplay = posplay = numposi = jumppos = mainvolume = 0;
    for (i = 0; i< 9; i++) memset(&lds_channel[i], 0,sizeof(LDS_Channel));
    memset(chandelay, 64, 9);
    memset(fmchip, 0, sizeof(fmchip));
    memset(lds_positions,0,1024*sizeof(LDS_Position));
    for (i = 0; i< 63; i++) memset(&lds_soundbank[i],0,sizeof(LDS_SoundBank));
    //fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

    // file load section (header)
    fread(&mode,1,1,f);
    if(mode > 2) {fclose(f); return false; }
    fread(&lds_speed,2,1,f);
    fread(&tempo,1,1,f);
    fread(&pattlen,1,1,f);
    for(i = 0; i < 9; i++) fread(&chandelay[i],1,1,f);
    fread(&regbd,1,1,f);

    // load patches
    fread(&numpatch,2,1,f);
    NUM_INSTRUMENTS = numpatch;
    for(i = 0; i < numpatch; i++) {
        sb = &lds_soundbank[i];
        fread(&sb->mod_misc,1,1,f); fread(&sb->mod_vol,1,1,f);
        fread(&sb->mod_ad,1,1,f); fread(&sb->mod_sr,1,1,f);
        fread(&sb->mod_wave,1,1,f); fread(&sb->car_misc,1,1,f);
        fread(&sb->car_vol,1,1,f); fread(&sb->car_ad,1,1,f);
        fread(&sb->car_sr,1,1,f); fread(&sb->car_wave,1,1,f);
        fread(&sb->feedback,1,1,f); fread(&sb->keyoff,1,1,f);
        fread(&sb->portamento,1,1,f); fread(&sb->glide,1,1,f);
        fread(&sb->finetune,1,1,f); fread(&sb->vibrato,1,1,f);
        fread(&sb->vibdelay,1,1,f); fread(&sb->mod_trem,1,1,f);
        fread(&sb->car_trem,1,1,f); fread(&sb->tremwait,1,1,f);
        fread(&sb->arpeggio,1,1,f);
        for(j = 0; j < 12; j++) fread(&sb->arp_tab[j],1,1,f);
        fread(&sb->start,2,1,f); fread(&sb->size,2,1,f);
        fread(&sb->fms,1,1,f); fread(&sb->transp,2,1,f);
        fread(&sb->midinst,1,1,f); fread(&sb->midvelo,1,1,f);
        fread(&sb->midkey,1,1,f); fread(&sb->midtrans,1,1,f);
        fread(&sb->middum1,1,1,f); fread(&sb->middum2,1,1,f);
    }

    // load positions
    fread(&numposi,2,1,f);
    //lds_positions = new Position[9 * numposi];
    for(i = 0; i < numposi; i++)
        for(j = 0; j < 9; j++) {
            //patnum is a pointer inside the pattern space, but patterns are 16bit
            //word fields anyway, so it ought to be an even number (hopefully) and
            //we can just divide it by 2 to get our array index of 16bit words.
            fread(&lds_positions[i * 9 + j].patnum,2,1,f);
            lds_positions[i * 9 + j].patnum = lds_positions[i * 9 + j].patnum/2;
            fread(&lds_positions[i * 9 + j].transpose,1,1,f);
        }

    // load patterns
    fseek(f,2,SEEK_CUR);// ignore # of digital sounds (not played by this player)
    //patterns = new unsigned short[(fp.filesize(f) - f->pos()) / 2 + 1];
    i = 0;
    while(1){
        if(!fread(&lds_patterns[i++],2,1,f)) break;
    }
    fclose(f);
    return true;
}
void lds_setregs(unsigned char reg, unsigned char val){
	if(fmchip[reg] == val) return;
	fmchip[reg] = val;
	opl2_write(reg, val);
}
void lds_setregs_adv(unsigned char reg, unsigned char mask,unsigned char val){
	lds_setregs(reg, (fmchip[reg] & mask) | val);
}
void lds_playsound(int inst_number, int channel_number, int tunehigh){
	unsigned int		regnum = CPlayer_op_table[channel_number];	// channel's OPL2 register
	unsigned char		volcalc, octave, remainder_192;
	unsigned short	freq;
	LDS_Channel		*c = &lds_channel[channel_number];		// current channel
	LDS_SoundBank		*i = &lds_soundbank[inst_number];		// current instrument
	Key_Hit[channel_number] = 1; //For visualizer bars
	// set fine tune
    tunehigh += ((i->finetune + c->finetune + 0x80) & 0xff) - 0x80;

	// arpeggio handling
	if(!i->arpeggio) {
		unsigned short	arpcalc = i->arp_tab[0] << 4;
		if(arpcalc > 0x800) tunehigh = tunehigh - (arpcalc ^ 0xff0) - 16;
		else tunehigh += arpcalc;
	}

	// glide handling
	if(c->glideto != 0) {
		c->gototune = tunehigh;
		c->portspeed = c->glideto;
        c->glideto = c->finetune = 0;
		return;
	}

	// set modulator registers
	lds_setregs(0x20 + regnum, i->mod_misc);
	volcalc = i->mod_vol;
	if(!c->nextvol || !(i->feedback & 1))c->volmod = volcalc;
	else c->volmod = (volcalc & 0xc0) | ((((volcalc & 0x3f) * c->nextvol) >> 6));

	if((i->feedback & 1) == 1 && allvolume != 0)
		lds_setregs(0x40 + regnum, ((c->volmod & 0xc0) | (((c->volmod & 0x3f) * allvolume) >> 8)) ^ 0x3f);
	else lds_setregs(0x40 + regnum, c->volmod ^ 0x3f);
	lds_setregs(0x60 + regnum, i->mod_ad);
	lds_setregs(0x80 + regnum, i->mod_sr);
	lds_setregs(0xe0 + regnum, i->mod_wave);

	// Set carrier registers
	lds_setregs(0x23 + regnum, i->car_misc);
	volcalc = i->car_vol;
	if(!c->nextvol) c->volcar = volcalc;
	else c->volcar = (volcalc & 0xc0) | ((((volcalc & 0x3f) * c->nextvol) >> 6));

	if(allvolume) lds_setregs(0x43 + regnum, ((c->volcar & 0xc0) | (((c->volcar & 0x3f) * allvolume) >> 8)) ^ 0x3f);
	else lds_setregs(0x43 + regnum, c->volcar ^ 0x3f);
	lds_setregs(0x63 + regnum, i->car_ad);
	lds_setregs(0x83 + regnum, i->car_sr);
	lds_setregs(0xe3 + regnum, i->car_wave);
	lds_setregs(0xc0 + channel_number, i->feedback);
	lds_setregs_adv(0xb0 + channel_number, 0xdf, 0);		// key off
	
	
	freq = lds_frequency[tunehigh];
	octave = lds_DIV1216[tunehigh];//tunehigh / (12 * 16) - 1;
	
	if(!i->glide) {
		if(!i->portamento || !c->lasttune) {
			lds_setregs(0xa0 + channel_number, freq & 0xff);
			lds_setregs(0xb0 + channel_number, (octave << 2) + 0x20 + (freq >> 8));
			c->lasttune = c->gototune = tunehigh;
		} else {
			c->gototune = tunehigh;
			c->portspeed = i->portamento;
			lds_setregs_adv(0xb0 + channel_number, 0xdf, 0x20);	// key on
		}
	} else {
		lds_setregs(0xa0 + channel_number, freq & 0xff);
		lds_setregs(0xb0 + channel_number, (octave << 2) + 0x20 + (freq >> 8));
		c->lasttune = tunehigh;
		c->gototune = tunehigh + ((i->glide + 0x80) & 0xff) - 0x80;	// set destination
		c->portspeed = i->portamento;
	}

	if(!i->vibrato) c->vibwait = c->vibspeed = c->vibrate = 0;
	else {
		c->vibwait = i->vibdelay;
		// PASCAL:    c->vibspeed = ((i->vibrato >> 4) & 15) + 1;
		c->vibspeed = (i->vibrato >> 4) + 2;
		c->vibrate = (i->vibrato & 15) + 1;
	}

	if(!(c->trmstay & 0xf0)) {
		c->trmwait = (i->tremwait & 0xf0) >> 3;
		// PASCAL:    c->trmspeed = (i->mod_trem >> 4) & 15;
		c->trmspeed = i->mod_trem >> 4;
		c->trmrate = i->mod_trem & 15;
		c->trmcount = 0;
	}

	if(!(c->trmstay & 0x0f)) {
		c->trcwait = (i->tremwait & 15) << 1;
		// PASCAL:    c->trcspeed = (i->car_trem >> 4) & 15;
		c->trcspeed = i->car_trem >> 4;
		c->trcrate = i->car_trem & 15;
		c->trccount = 0;
	}

	c->arp_size = i->arpeggio & 15;
	c->arp_speed = i->arpeggio >> 4;
	memcpy(c->arp_tab, i->arp_tab, 12);
	c->keycount = i->keyoff;
	c->nextvol = c->glideto = c->finetune = c->vibcount = c->arp_pos = c->arp_count = 0;
}
void lds_get_instrument(int inst_number){
    //ADSR and tremolo can be set to both opl2 operators
    //We select only one for the XM instrumet.
    LDS_SoundBank *i = &lds_soundbank[inst_number];
    byte ca = i->car_ad >> 4; byte cd = i->car_ad & 0x0F; byte cs = (i->car_sr >> 4)&0x0F; byte cr = i->car_sr & 0x0F;
    byte ma = i->mod_ad >> 4; byte md = i->mod_ad & 0x0F; byte ms = (i->mod_sr >> 4)&0x0F; byte mr = i->mod_sr & 0x0F;
    byte j;
    // Choose one
    ca=15-ca;cd=15-cd;cr=15-cr;ma=15-ma;md=15-md;mr=15-mr;
    if (ca == 15) { cs = 0;} if (cd == 15) cs = 15;
    if (ma == 15) { ms = 0;} if (md == 15) ms = 15;
    if (cr == 15) cr = 99; if (mr == 15) mr = 99;
    //if (cs == 15 && ms == 15) {cs = 0; ms = 0;}
    /*
    printf("\n\nConvert Instrument %i\n",inst_number);
    printf("Vibrato %i %i\nTremolo c %i m %i\n",i->vibrato & 15,i->vibrato >> 4,i->car_trem,i->mod_trem);
    printf("Arpeggio %i  ",i->arpeggio);for(j = 0; j < 12; j++)printf(" %i,",i->arp_tab[j]);
    printf("Portamento %i  ",i->portamento);
    printf("Glide %i  ",i->glide);
    printf("Finetune %i  ",i->finetune);
    printf("\n\n");
    printf("Select envelope\n");
    printf("0: CARRIER   A %2i  D %2i  SVol %2i  R %2i\n",ca,cd,cs,cr);
    printf("1: MODULATOR A %2i  D %2i  SVol %2i  R %2i\n\n",ma,md,ms,mr);
    */

    //Select ADSR, try to get  "shapes" and avoid flat envelopes.
    if (cs == 15 && ms == 15) {
        if (ca > ma) {XM_Ins_A = ca; XM_Ins_D = cd; XM_Ins_S = cs; XM_Ins_R = cr;}
        else if (ma > ca) {XM_Ins_A = ma; XM_Ins_D = md; XM_Ins_S = ms; XM_Ins_R = mr;}
    } else {
        if (cs == ms) {
            if (ca > ma) {XM_Ins_A = ca; XM_Ins_D = cd; XM_Ins_S = cs; XM_Ins_R = cr;}
            else if (ma > ca) {XM_Ins_A = ma; XM_Ins_D = md; XM_Ins_S = ms; XM_Ins_R = mr;}
        } else {
            if (cs > 0 && cs != 15) {XM_Ins_A = ca; XM_Ins_D = cd; XM_Ins_S = cs; XM_Ins_R = cr;}
            if (ms > 0 && ms != 15) {XM_Ins_A = ma; XM_Ins_D = md; XM_Ins_S = ms; XM_Ins_R = mr;}
        }
    }
    //get tremolo, use the operator with higher value, or discard the one with no tremolo
    if (i->car_trem > i->mod_trem) {
        XM_Ins_trem_speed = i->car_trem >> 4;
        XM_Ins_trem_rate = i->car_trem & 0x0F;
    } else {
        XM_Ins_trem_speed = i->mod_trem >> 4;
        XM_Ins_trem_rate = i->mod_trem & 0x0F;
    }
    XM_Ins_car_misc = i->mod_misc;
    XM_Ins_tremwait = i->tremwait;
    XM_Ins_keyoff = i->keyoff;
    XM_Ins_portamento = i->portamento;
    XM_Ins_glide = i->glide;
    XM_Ins_finetune = i->finetune;
    XM_Ins_vibrato_depth = i->vibrato & 0x0F;
    XM_Ins_vibrato_rate = (i->vibrato >> 4);
    XM_Ins_vibdelay = i->vibdelay;
    XM_Ins_arpeggio = i->arpeggio;
    memcpy(XM_Ins_arp_tab,i->arp_tab,12);
    XM_Ins_MIDI = i->midinst;
    //printf("2: SELECTED A %2i  D %2i  SVol %2i  R %2i\n\n",XM_Ins_A,XM_Ins_D,XM_Ins_S,XM_Ins_R);
    //wait();
}
void lds_test_all_instruments(){
    word i = numpatch;
    word time = 2*60;
    while(i){
        if (time == 0) {time = 2*60;i--;}
        if (time == 2*60) {
            printf("\n instrument %i \n",i);
            lds_playsound(i,0,1024);
        }
        time--;
        Wait_Vsync();
    }
}
byte IT_channel_vib[9] = {0};
byte IT_channel_gli[9] = {0};
void CldsPlayer_update(void){
	unsigned short	comword, freq, octave, chan, tune, wibc, tremc, arpreg;
	byte			vbreak;
	unsigned char		level, regnum, comhi, comlo;
	int			i;
    int end_song = 0;
    LDS_Channel		*c;

    if (end_song && XM_pat) playing = 0;
	if(!playing) return ;//false;

	// handle fading
	if(fadeonoff){
		if(fadeonoff <= 128) {//64.562
			
			if (allvolume > fadeonoff || allvolume == 0) allvolume -= fadeonoff;
			else {
				allvolume = 1;
				fadeonoff = 0;
				if(hardfade != 0) {
					playing = false;
					hardfade = 0;
					for(i = 0; i < 9; i++) lds_channel[i].keycount = 1;
				}
			}
		} else {
			if(((allvolume + (0x100 - fadeonoff)) & 0xff) <= mainvolume) allvolume += 0x100 - fadeonoff;
			else {
				allvolume = mainvolume;
				fadeonoff = 0;
			}
		}
	}
	// handle channel delay
	for(chan = 0; chan < 9; chan++) {
		c = &lds_channel[chan];
		if(c->chancheat.chandelay){
			if(!(--c->chancheat.chandelay)) lds_playsound(c->chancheat.sound, chan, c->chancheat.high);
		}
	}

	// handle notes
	if(!tempo_now) {
		vbreak = false;
        printf("%2i ",pattplay);
		for(chan = 0; chan < 9; chan++) {
			c = &lds_channel[chan];
			if(!c->packwait) {
				unsigned short	patnum = lds_positions[posplay * 9 + chan].patnum;
				unsigned char	transpose = lds_positions[posplay * 9 + chan].transpose;
				comword = lds_patterns[patnum + c->packpos];
				comhi = comword >> 8; comlo = comword & 0xff;
				if(comword){
                    if(comhi == 0x80) {
                        printf("_OFF ");
                        IT_channel_vib[chan] = 0;
                        write_IT_event(chan,0,0xFD,0xFF,0,0,0);
                        c->packwait = comlo;
                    } else {
						if(comhi >= 0x80) {
							switch(comhi) {
                                case 0xff: //volume
                                    printf("VSET ");
                                    write_IT_event(chan,0,0xFF,0xFF,64,0,0);
									c->volcar = (((c->volcar & 0x3f) * comlo) >> 6) & 0x3f;
									if(fmchip[0xc0 + chan] & 1) c->volmod = (((c->volmod & 0x3f) * comlo) >> 6) & 0x3f;
								break;
                                case 0xfe:
                                    tempo = comword & 0x3f;
                                    printf("--T_ ");
                                    IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0x01,comword);
                                    break;
                                case 0xfd:
                                    c->nextvol = comlo;
                                    printf("%02XVO ",c->nextvol);
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    break;
								//full keyoff here
                                case 0xfc:
                                    if (XM_pat) playing = false; //for > 1 pattern songs
                                    printf("_END ");
                                    end_song = 1; //for 1 pattern songs
                                    IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFC,0xFF,0,0,0);
                                    //lds_setregs_adv(0xb0 + chan, 0xdf, 0);
                                    break;
                                case 0xfb:
                                    c->keycount = 1;
                                    printf("_OFF ");
                                    IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFD,0xFF,0,0,0);
                                    break;
                                case 0xfa: //pattern break
                                    printf("_B__ ");
                                    IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
									vbreak = true;
									jumppos = (posplay + 1) & lds_maxpos;
								break;
                                case 0xf9: //pattern jump (always song end and loop)
                                    playing = false;
                                    printf("_B__ ");
                                    IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0x02,0);//jump to 0
									vbreak = true;
									jumppos = comlo & lds_maxpos;
									jumping = 1;
                                    if(jumppos < posplay) songlooped = true;
								break;
                                case 0xf8:
                                    c->lasttune = 0;
                                    printf("__TU ");
                                    IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    break;
                                case 0xf7: //vibrato
                                    printf("_VI_ ");
                                    IT_channel_vib[chan] = comlo;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0x08,comlo);
									c->vibwait = 0;
									// PASCAL: c->vibspeed = ((comlo >> 4) & 15) + 2;
									c->vibspeed = (comlo >> 4) + 2;
                                    c->vibrate = (comlo & 15) + 1;
								break;
                                case 0xf6:
                                    c->glideto = comlo;
                                    printf("---- ",comlo);
                                    IT_channel_vib[chan] = 0;
                                    IT_channel_gli[chan] = comlo;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    break;
                                case 0xf5:
                                    c->finetune = comlo;
                                    printf("_FTU ");
                                    //IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    break;
                                case 0xf4: //FADE EFFECT
                                    printf("FADE ");
                                    //IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
									if(!hardfade) {
										allvolume = mainvolume = comlo;
										fadeonoff = 0;
									}
								break;
                                case 0xf3:
                                    printf("FADE ");
                                    //IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    if(!hardfade) fadeonoff = comlo;
                                    break;
                                case 0xf2:
                                    printf("TREM ");
                                    IT_channel_vib[chan] = 0;
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    c->trmstay = comlo;
                                    break;
								case 0xf1:	// panorama
								case 0xf0:	// progch
									// MIDI commands (unhandled)
                                    printf("MIDI ");
                                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
								break;
								default:
                                    if(comhi < 0xa0) {
                                        c->glideto = comhi & 0x1f;
                                        printf("---- ");
                                        IT_channel_gli[chan] = comhi & 0x1f;
                                        IT_channel_vib[chan] = 0;
                                        write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    }
                                    else {
                                        printf("---- ",c->gototune);
                                        //IT_channel_vib[chan] = 0;
                                        write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    }
								break;
							}
						} else {
							unsigned char	sound;
							unsigned short	high;
							signed char	transp = transpose;
							asm shl transp,1
							asm sar transp,1
							if(transpose & 128) {
								sound = (comlo + transp) & lds_maxsound;
								high = comhi << 4;
							} else { sound = comlo & lds_maxsound; high = (comhi + transp) << 4;}

                            if(!chandelay[chan]) {
                                lds_playsound(sound, chan, high);
                            } else {
								c->chancheat.chandelay = chandelay[chan];
								c->chancheat.sound = sound;
								c->chancheat.high = high;
                            }
                                if (IT_channel_gli[chan]){
                                    printf("%02XGL ",comhi);
                                    if(transpose & 128) {
                                        write_IT_event(chan,0,comhi,0xFF,0,0x07,IT_channel_gli[chan]);
                                    } else {
                                        write_IT_event(chan,0,comhi+transp,0xFF,0,0x07,IT_channel_gli[chan]);
                                    }
                                    IT_channel_gli[chan] = 0;
                                    IT_channel_vib[chan] = 0;
                                } else {
                                    if (c->nextvol){
                                        printf("%02XVO ",c->nextvol);
                                        IT_channel_vib[chan] = 0;
                                        write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                                    } else {
                                        byte vol = 0;
                                        LDS_SoundBank *ldsinst = &lds_soundbank[sound];
                                        printf("%2i%02X ",sound,comlo);
                                        if (!c->volcar) vol = ldsinst->car_vol;
                                        else vol = c->volcar;
                                        IT_channel_vib[chan] = 0;
                                        if(transpose & 128) {
                                            write_IT_event(chan,0,comhi,comlo,vol,0,0);
                                        } else {
                                            write_IT_event(chan,0,comhi + transp,comlo,vol,0,0);
                                        }
                                    }
                                }
                        }
					}
                } else if (IT_channel_vib[chan]){
                    printf("VB%02X ",IT_channel_vib[chan]);
                    write_IT_event(chan,0,0xFF,0xFF,0,0x08,IT_channel_vib[chan]);
                } else if (IT_channel_gli[chan]){
                    printf("%02XGL ",IT_channel_gli[chan]);
                    write_IT_event(chan,0,0xFF,0xFF,0,0x07,IT_channel_gli[chan]);
                } else if (0){
                    //arpegio continue?.. this is not used in any tune
                    //they use internal instrument arpeggios
                } else {
                    printf("---- ");
                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                }
				c->packpos++;
            } else {
                c->packwait--;
                if (IT_channel_vib[chan]){
                    printf("VB%02X ",IT_channel_vib[chan]);
                    write_IT_event(chan,0,0xFF,0xFF,0,0x08,IT_channel_vib[chan]);
                } else if (IT_channel_gli[chan]){
                    printf("%02XGL ",IT_channel_gli[chan]);
                    write_IT_event(chan,0,0xFF,0xFF,0,0x07,IT_channel_gli[chan]);
                } else if (0){
                    //arpegio continue?.. this is not used in any tune
                    //they use internal instrument arpeggios
                } else {
                    printf("---- ");
                    write_IT_event(chan,0,0xFF,0xFF,0,0,0);
                }
            }
		}
        write_IT_event(0,1,0,0,0,0,0);
        printf("\n");
		tempo_now = tempo;

		//The continue table is updated here, but this is only used in the
		//original player, which can be paused in the middle of a song and then
		//unpaused. Since AdPlug does all this for us automatically, we don't
		//have a continue table here. The continue table update code is noted
		//here for reference only.
	
		//if(!pattplay) {
		//  conttab[speed & maxcont].position = posplay & 0xff;
		//  conttab[speed & maxcont].tempo = tempo;

        pattplay++;
        XM_row++;
        if(vbreak) {
            write_IT_pattern(XM_row,XM_pat);
            pattplay = 0; XM_row = 0; XM_pat++;
			for(i = 0; i < 9; i++) lds_channel[i].packpos = lds_channel[i].packwait = 0;
			posplay = jumppos;
            printf("\n\n");
		} else {
            if(pattplay >= pattlen) {
                write_IT_pattern(XM_row,XM_pat);
                pattplay = 0; XM_row = 0; XM_pat++;
				for(i = 0; i < 9; i++) lds_channel[i].packpos = lds_channel[i].packwait = 0;
				posplay = (posplay + 1) & lds_maxpos;
                printf("\n\n");
            }
		}
	} else tempo_now--;

	// make effects
	for(chan = 0; chan < 9; chan++) {
		c = &lds_channel[chan];
		regnum = CPlayer_op_table[chan];
		if(c->keycount > 0) {
			if(c->keycount == 1) lds_setregs_adv(0xb0 + chan, 0xdf, 0);
			c->keycount--;
		}

		// arpeggio
		if(c->arp_size == 0) arpreg = 0;
		else {
			arpreg = c->arp_tab[c->arp_pos] << 4;
			if(arpreg == 0x800) {
				if(c->arp_pos > 0) c->arp_tab[0] = c->arp_tab[c->arp_pos - 1];
				c->arp_size = 1; c->arp_pos = 0;
				arpreg = c->arp_tab[0] << 4;
			}

			if(c->arp_count == c->arp_speed) {
				c->arp_pos++;
				if(c->arp_pos >= c->arp_size) c->arp_pos = 0;
				c->arp_count = 0;
			} else c->arp_count++;
		}

		// glide & portamento
		if(c->lasttune && (c->lasttune != c->gototune)) {
			if(c->lasttune > c->gototune) {
				if(c->lasttune - c->gototune < c->portspeed) c->lasttune = c->gototune;
				else c->lasttune -= c->portspeed;
			} else {
				if(c->gototune - c->lasttune < c->portspeed) c->lasttune = c->gototune;
				else c->lasttune += c->portspeed;
			}
		
			if(arpreg >= 0x800) arpreg = c->lasttune - (arpreg ^ 0xff0) - 16;
			else arpreg += c->lasttune;
			freq = lds_frequency[arpreg];
			octave = lds_DIV1216[arpreg];//arpreg / (12 * 16) - 1;
			lds_setregs(0xa0 + chan, freq & 0xff);
			lds_setregs_adv(0xb0 + chan, 0x20, ((octave << 2) + (freq >> 8)) & 0xdf);
		} else { // vibrato
			if(!c->vibwait) {
				if(c->vibrate) {
					wibc = lds_vibtab[c->vibcount & 0x3f] * c->vibrate;
				
					if((c->vibcount & 0x40) == 0) tune = c->lasttune + (wibc >> 8);
					else tune = c->lasttune - (wibc >> 8);
				
					if(arpreg >= 0x800) tune = tune - (arpreg ^ 0xff0) - 16;
					else tune += arpreg;
				
					freq = lds_frequency[tune];
					octave = lds_DIV1216[tune];//tune / (12 * 16) - 1;
					lds_setregs(0xa0 + chan, freq & 0xff);
					lds_setregs_adv(0xb0 + chan, 0x20, ((octave << 2) + (freq >> 8)) & 0xdf);
					c->vibcount += c->vibspeed;
				} else {
					if(c->arp_size != 0) {	// no vibrato, just arpeggio
						if(arpreg >= 0x800) tune = c->lasttune - (arpreg ^ 0xff0) - 16;
						else tune = c->lasttune + arpreg;
				
						freq = lds_frequency[tune];
						octave = lds_DIV1216[tune];//tune / (12 * 16) - 1;
						lds_setregs(0xa0 + chan, freq & 0xff);
						lds_setregs_adv(0xb0 + chan, 0x20, ((octave << 2) + (freq >> 8)) & 0xdf);
					}
				}
			} else { // no vibrato, just arpeggio
				c->vibwait--;

				if(c->arp_size != 0) {
					if(arpreg >= 0x800) tune = c->lasttune - (arpreg ^ 0xff0) - 16;
					else tune = c->lasttune + arpreg;
				
					freq = lds_frequency[tune];
					octave = lds_DIV1216[tune];//tune / (12 * 16) - 1;
					lds_setregs(0xa0 + chan, freq & 0xff);
					lds_setregs_adv(0xb0 + chan, 0x20, ((octave << 2) + (freq >> 8)) & 0xdf);
				}
			}
		}

		// tremolo (modulator)
		if(!c->trmwait) {
			if(c->trmrate) {
				tremc = lds_tremtab[c->trmcount & 0x7f] * c->trmrate;
				if((tremc >> 8) <= (c->volmod & 0x3f)) level = (c->volmod & 0x3f) - (tremc >> 8);
				else level = 0;

				if(allvolume != 0 && (fmchip[0xc0 + chan] & 1))
					lds_setregs_adv(0x40 + regnum, 0xc0, ((level * allvolume) >> 8) ^ 0x3f);
				else lds_setregs_adv(0x40 + regnum, 0xc0, level ^ 0x3f);

				c->trmcount += c->trmspeed;
			} else {
				if(allvolume != 0 && (fmchip[0xc0 + chan] & 1))
					lds_setregs_adv(0x40 + regnum, 0xc0, ((((c->volmod & 0x3f) * allvolume) >> 8) ^ 0x3f) & 0x3f);
				else
					lds_setregs_adv(0x40 + regnum, 0xc0, (c->volmod ^ 0x3f) & 0x3f);
			}
		} else {
			c->trmwait--;
			if(allvolume != 0 && (fmchip[0xc0 + chan] & 1))
				lds_setregs_adv(0x40 + regnum, 0xc0, ((((c->volmod & 0x3f) * allvolume) >> 8) ^ 0x3f) & 0x3f);
		}

		// tremolo (carrier)
		if(!c->trcwait) {
			if(c->trcrate) {
				tremc = lds_tremtab[c->trccount & 0x7f] * c->trcrate;
				if((tremc >> 8) <= (c->volcar & 0x3f)) level = (c->volcar & 0x3f) - (tremc >> 8);
				else level = 0;

				if(allvolume != 0)
					lds_setregs_adv(0x43 + regnum, 0xc0, ((level * allvolume) >> 8) ^ 0x3f);
				else lds_setregs_adv(0x43 + regnum, 0xc0, level ^ 0x3f);
				c->trccount += c->trcspeed;
			} else {
				if(allvolume != 0)
					lds_setregs_adv(0x43 + regnum, 0xc0, ((((c->volcar & 0x3f) * allvolume) >> 8) ^ 0x3f) & 0x3f);
				else
					lds_setregs_adv(0x43 + regnum, 0xc0, (c->volcar ^ 0x3f) & 0x3f);
			}
		} else {
			c->trcwait--;
			if(allvolume != 0) lds_setregs_adv(0x43 + regnum, 0xc0, ((((c->volcar & 0x3f) * allvolume) >> 8) ^ 0x3f) & 0x3f);
		}
	}
	//Return
	return;// (!playing || songlooped) ? false : true;
}

