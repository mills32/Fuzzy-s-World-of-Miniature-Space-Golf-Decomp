#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;

#define IMG_WIDTH  320
#define IMG_HEIGHT 200
#define IMG_PIXELS (IMG_WIDTH * IMG_HEIGHT)  /* 64000 */
byte *VGA=(byte *)0xA0000000L;

byte Global_Strings[16][81] = {
    //{"FUZZY"},//Name of RES files, original game checks if FUZZY0-6.RES are there
    {"                                                                                \0"},
    {"Open Fuzzy's World (Fuzzy's World of Miniature Space Golf)  -  Version 1.0      \0"},
    {"Original 1995 Pixel Painters Corporation.                                       "},
    {"Reverse engineered 2027 Mills.                                                  \0"},
    {"Sound card detected...                                                          \0"},
    {"No Sound card detected...                                                       \0"},
    {"Microsoft compatible mouse detected...                                          \0"},
    {"No Microsoft compatible mouse detected...                                       \0"},
    {"Sound disabled...                                                               \0"},
    {"Music disabled...                                                               \0"},
    {"Sound effects disabled...                                                       \0"},
    {"Mouse doubler enabled...                                                        \0"},
    {"Sound card forced to DMA1...                                                    \0"},
    {"Sound card forced to DMA3...                                                    \0"},
    {"For a list of the command line options, type FUZZY /? at the DOS prompt.        \0"},
    {"Press any key to begin...                                                       \0"},
};

byte Fuzzy_CMD[] = {"\
Command Line Options:\n\
    /?        Displays this screen.\n\
    /M2       Mouse doubler enabled. Use this if your mouse will only move half\n\
              way across the screen.\n\
    /S-       Disables all music and sound.\n\
    /M-       Disables the background music.\n\
    /F-       Disables sound effects.\n\
    /DMA1     Forces the program to use DMA channel 1 for the sound card.\n\
    /DMA3     Forces the program to use DMA channel 3 for the sound card.\0\
Pixel Painters Corporation, P.O. Box 2847, Merrifield, VA, 22116, (703) 222-0568\0"
};


byte Wait_kbhit(){
    byte val = 0;
    asm mov ax,0x0000
    asm int 0x16
    asm mov val,al
    return val;
}

void Clearkb(){
    asm mov ax,0x0C00
    asm int 21h
}

void Wait_Vsync(){
    //Try to catch Vertical retrace
    asm mov	dx,0x3DA; asm mov bl,0x08;
    WaitNotVsync: asm in al,dx; asm test al,bl; asm jnz WaitNotVsync;
    WaitVsync:    asm in al,dx; asm test al,bl; asm jz WaitVsync;
    //while( inp( INPUT_STATUS_0 ) & 0x08 );
    //while( !(inp( INPUT_STATUS_0 ) & 0x08 ) );
}

void Set_Text_Mode(){
    asm mov ax,0x0003;asm int 0x10;
}

//PALETTE
byte Fuzzy_Palette[768] = {0};
byte Fuzzy_Bank = 0;
byte *Fuzzy_RAM_Buffer;
byte *Fuzzy_RAM_Buffer0;
//BACKGROUND ANIMATION
word Fuzzy_BKG_Animation_Frames;
byte Fuzzy_BKG_Animation_Frame;
word Fuzzy_BKG_Animation_TSize;
dword Fuzzy_Data_Offset = 0;
byte *Fuzzy_BKG_Animation_Data0;
byte *Fuzzy_BKG_Animation_Data1;
byte *Fuzzy_BKG_Animation_Data2;
byte *Fuzzy_BKG_Animation_Data3;
byte *Fuzzy_BKG_Animation_Data4;
byte *Fuzzy_BKG_Animation_FrameData[256];

void Fuzzy_Exit(){
    //EXIT DOS
    if (Fuzzy_BKG_Animation_Data0) free(Fuzzy_BKG_Animation_Data0);
    if (Fuzzy_BKG_Animation_Data1) free(Fuzzy_BKG_Animation_Data1);
    if (Fuzzy_BKG_Animation_Data2) free(Fuzzy_BKG_Animation_Data2);
    if (Fuzzy_BKG_Animation_Data3) free(Fuzzy_BKG_Animation_Data3);
    if (Fuzzy_BKG_Animation_Data4) free(Fuzzy_BKG_Animation_Data4);
    if (Fuzzy_RAM_Buffer) free(Fuzzy_RAM_Buffer);
    asm mov	ax,0x4C00;
    asm int	0x21; //EXIT
}

void Allocate_Memory(){
    //ALLOCATE MEMORY
    word _SEG,_OFF,_PAD,_SOF;
    Fuzzy_BKG_Animation_Data0 = (byte*) calloc(65535,1);
    if (!Fuzzy_BKG_Animation_Data0) {Set_Text_Mode();printf("Can't allocate RAM\n");Fuzzy_Exit();}
    Fuzzy_BKG_Animation_Data1 = (byte*) calloc(65535,1);
    if (!Fuzzy_BKG_Animation_Data1) {Set_Text_Mode();printf("Can't allocate RAM\n");Fuzzy_Exit();}
    Fuzzy_BKG_Animation_Data2 = (byte*) calloc(65535,1);
    if (!Fuzzy_BKG_Animation_Data2) {Set_Text_Mode();printf("Can't allocate RAM\n");Fuzzy_Exit();}
    Fuzzy_BKG_Animation_Data3 = (byte*) calloc(65535,1);
    if (!Fuzzy_BKG_Animation_Data3) {Set_Text_Mode();printf("Can't allocate RAM\n");Fuzzy_Exit();}
    Fuzzy_BKG_Animation_Data4 = (byte*) calloc(32768,1);
    if (!Fuzzy_BKG_Animation_Data4) {Set_Text_Mode();printf("Can't allocate RAM\n");Fuzzy_Exit();}
    Fuzzy_RAM_Buffer = (byte*) calloc((320*200)+1024,1);
    if (!Fuzzy_RAM_Buffer) {Set_Text_Mode();printf("Can't allocate RAM\n");Fuzzy_Exit();}
    //Align buffer so offset = 0;
    _SEG = ((unsigned)((unsigned long)(Fuzzy_RAM_Buffer) >> 16));
    _OFF = ((unsigned)((unsigned long)(Fuzzy_RAM_Buffer) & 0xFFFF));
    _PAD = 16-(_OFF&15); _SOF = (_OFF + _PAD)>>4;
    _SEG += _SOF; _OFF = 0x0000;
    Fuzzy_RAM_Buffer0 = ((void far *)((unsigned long)(_SEG) << 16 | (unsigned)(_OFF)));
}

int Init_Game(){
    textcolor(15); textbackground(1);
    cprintf("%s",Global_Strings[1]);
    textcolor(7); textbackground(0);
    cprintf("%s",Global_Strings[2]);
    cprintf("%s",Global_Strings[3]);
    cprintf("%s",Global_Strings[0]);
    //Detect sound and mouse and command line options
    //Oh, I something something money. Come on, give me lots of money
    cprintf("%s",Global_Strings[4]);
    cprintf("%s",Global_Strings[6]);

    //Info commands and press key to begin
    cprintf("%s",Global_Strings[0]);
    cprintf("%s",&Global_Strings[14]);
    cprintf("%s",Global_Strings[0]);
    textcolor(15); textbackground(0);
    cprintf("%s",Global_Strings[15]);
    while(!kbhit());
    Clearkb();
    return 1;
}

void Fuzzy_BlitFrame(){
    asm push di; asm push si; asm push ds;

    //Try to catch Vertical retrace
    asm mov	dx,0x3DA; asm mov bl,0x08;
    WaitNotVsync: asm in al,dx; asm test al,bl; asm jnz WaitNotVsync;
    WaitVsync:    asm in al,dx; asm test al,bl; asm jz WaitVsync;

    //Set memcpy
    asm lds si,Fuzzy_RAM_Buffer0; //Source DS:[SI]
    asm mov ax,0xA000; asm mov es,ax; asm mov di,0; //Destination VGA ES:[DI]

    //memcpy
    asm CLD;
    asm mov cx,(320*200)/2;
    asm rep movsw;

    asm pop ds; asm pop si; asm pop di;
}

void set_palette(unsigned char *palette){
    asm push es; asm push ds; asm push si;
    asm mov	dx,0x03C8; asm mov al,0; asm out dx,al;
    asm lds si,palette
        asm mov dx,0x03C9 //Palete register
        asm mov cx,256*3
            _in_loop:
                       asm LODSB //Load byte from DS:SI into AL, then advance SI
                       asm out dx,al
        asm loop _in_loop
        asm pop es; asm pop ds; asm pop si;
}

void Read_Animation(FILE *in){
    int i = 0;
    Fuzzy_Data_Offset = 0;
    fread(&Fuzzy_BKG_Animation_Frames, 1, 2, in);
    fread(&Fuzzy_BKG_Animation_TSize, 1, 2, in);
    fread(Fuzzy_Palette, 1, 768, in);
    set_palette(Fuzzy_Palette);
    for (i = 0; i != Fuzzy_BKG_Animation_Frames+1; i++){
        word size;
        fread(&size, 1, 2, in);
        if (Fuzzy_Bank < 4){
            if (Fuzzy_Data_Offset + (unsigned long)size > 65535) {Fuzzy_Bank++;Fuzzy_Data_Offset = 0;}
        }
        if (Fuzzy_Bank == 4){
            if (Fuzzy_Data_Offset + (unsigned long)size > 32768) {
                Set_Text_Mode();
                printf("Animation is too big. Bank %i;  frames %i\n",Fuzzy_Bank,i);
                Fuzzy_Exit();
            }
        }
        if (Fuzzy_Bank == 0){
            fread(Fuzzy_BKG_Animation_Data0+Fuzzy_Data_Offset,1,size,in);
            Fuzzy_BKG_Animation_FrameData[i] = Fuzzy_BKG_Animation_Data0 + Fuzzy_Data_Offset;
        } else if (Fuzzy_Bank == 1){
            fread(Fuzzy_BKG_Animation_Data1+Fuzzy_Data_Offset,1,size,in);
            Fuzzy_BKG_Animation_FrameData[i] = Fuzzy_BKG_Animation_Data1 + Fuzzy_Data_Offset;
        } else if (Fuzzy_Bank == 2){
            fread(Fuzzy_BKG_Animation_Data2+Fuzzy_Data_Offset,1,size,in);
            Fuzzy_BKG_Animation_FrameData[i] = Fuzzy_BKG_Animation_Data2 + Fuzzy_Data_Offset;
        } else if (Fuzzy_Bank == 3){
            fread(Fuzzy_BKG_Animation_Data3+Fuzzy_Data_Offset,1,size,in);
            Fuzzy_BKG_Animation_FrameData[i] = Fuzzy_BKG_Animation_Data3 + Fuzzy_Data_Offset;
        } else if (Fuzzy_Bank == 4){
            fread(Fuzzy_BKG_Animation_Data4+Fuzzy_Data_Offset,1,size,in);
            Fuzzy_BKG_Animation_FrameData[i] = Fuzzy_BKG_Animation_Data4 + Fuzzy_Data_Offset;
        } else {
            Set_Text_Mode();
            printf("Animation is too big. Bank %i;  frames %i\n",Fuzzy_Bank,i);
            Fuzzy_Exit();
        }
        Fuzzy_Data_Offset += size;
    }
}

//Read file from RES
int Read_Animation_From_RES(char *file_name, byte debug_mode, char *resource_name){
    unsigned short nroffiles;
    char filename[16];      // 12 chars + null terminator
    char cleanname[16];
    unsigned long offset;
    word length;
    word length1;
    byte image_number = 0;
    int x, y, i;
    FILE *in = fopen(file_name, "rb");

    if (!in) {printf("Can't find %s\n", file_name);return 0;}

    // Read number of files
    fread(&nroffiles, 2, 1, in);
    //Extract files
    for (x = 0; x < nroffiles; x++) {
        // Read filename (12 bytes)
        fseek(in, (x * 22) + 3, SEEK_SET);
        fread(filename, 1, 12, in);
        filename[12] = '\0';

        // Strip zero padding
        for (y = 0; y < 12; y++) {
            if (filename[y] == 0) break;
            cleanname[y] = filename[y];
        }
        cleanname[y] = '\0';
        if (!debug_mode){//Load filename, like original game
            if (!strcmp(filename,resource_name)){//IF found file
                // Read offset and length
                fread(&offset, 4, 1, in);
                fread(&length, 2, 1, in);
                fread(&length1, 2, 1, in);
                fseek(in, offset, SEEK_SET);
                //read resource image
                Read_Animation(in);
                fclose(in);
                return 1;
            }
        } else { //load an image (ANI/SPF) at position defined by debug_mode variable
            if (strstr(filename,".SPF") || strstr(filename,".ANI")){
                if (image_number == debug_mode-1){
                    // Read offset and length
                    fread(&offset, 4, 1, in);
                    fread(&length, 2, 1, in);
                    fread(&length1, 2, 1, in);
                    fseek(in, offset, SEEK_SET);
                    //read resource image
                    Read_Animation(in);
                    fclose(in);
                    return 1;
                }
                image_number++;
            }
        }
    }
    fclose(in);
    return 0;
}

//###########################################
//#   Decode SPF / ANI from Fuzzy's World   #
//###########################################
word Decode_Image(byte *src, byte *dst){
    byte *si      = src;
    byte *di      = dst;
    byte *end     = dst + IMG_PIXELS;
    word ax,bx,cx;
    int skip = 0;
    byte lo,hi;
    byte color = 0;
    int offset;
    word pos;
    while (di < end) {
        byte code = *si;
        byte op   = (code >> 5) & 0x7;
        switch (op) {
            case 0:   // Literal copy: N bytes
                cx = (code & 0x1F) + 1;
                si++;
                while (cx--){ *di++ = *si++;}
                break;
            case 1:   // Long skip (up to 8192 pixels)
                ax = (word)(((word)si[0] << 8) | si[1]);
                skip = (ax & 0x1FFF) + 1;
                si += 2;
                di += skip;
                break;
            case 2:   // Short skip (up to 32 pixels)
                skip = (*si & 0x1F) + 1;
                si++;
                di += skip;
                break;
            case 3:   // word fill, repeat 16 bit word
                lo = 0;hi = si[1];
                cx = (int)((((word)si[0] << 8) | si[1]) & 0x1FFF);
                si += 2;
                lo = si[0]; hi = si[1];
                si += 2;
                while (cx-- /*&& di + 1 < end*/) {*di++ = lo;*di++ = hi;}
                break;
            case 4:   // Byte fill: repeat one color N times
                bx = (int)((((word)si[0] << 8) | si[1]) & 0x1FFF) + 1;
                color = si[2];
                memset(di,color,bx);
                si += 3; di += bx;
                break;
            case 5:   // Scanline fill: fixed color, short count
                color = si[1];
                cx    = (si[0] & 0x1F) + 1;
                si+=2;
                //if (di + cx > end) cx = (int)(end - di);
                memset(di, color,cx);
                di += cx;
                break;
            case 6:   // Back-reference (relative offset)
                ax = (word)((((word)si[0] << 8) | si[1]) & 0x1FFF);
                si += 2;
                offset = (int)ax + 1;
                cx     = (*si++ & 0xFF) + 1;
                pos = (di - dst) - offset;
                while (cx-- /*&& di < end*/) {*di++ = dst[pos]; pos++;}
                break;
            case 7: //Back-reference (absolute offset)
                cx = (*si & 0x1F) + 1;
                si++;
                bx = (word)(si[0] | ((word)si[1] << 8));
                si += 2;
                while (cx-- /*&& di < end*/) *di++ = dst[bx++];
                break;
            default:  si++; break;
        }
    }
    return (word)(si - src);
}
void Decode_Image_ASM(byte *compressed_data, byte *destination_buffer){
    byte *data = compressed_data;
    byte *dest = destination_buffer;

    asm push di; asm push si; asm push ds;

    asm cld // DF=0: inc SI/DI

    asm lds si,data // DS:SI = source compressed data
    asm les di,dest // ES:DI = destination
    asm xor di,di
    LOOP_READ_NEXT_CODE://Read code, get 3 upper bits and jump (switch-case in asm)
    asm xor ah, ah;
    asm mov al, byte ptr ds:[si]; // leer byte de control (SI no avanza aún)
    asm shl ax,1; asm shl ax,1; asm shl ax,1; // bits 7-5 de AL suben a AH bits 2-0
    asm mov bl,ah; asm and bx, 0x0007;      // BL = opcode (0-7) aislar 3 bits
        /*
        // salto indirecto via tabla en CS
        jmp word ptr cs:[bx + offset DECODE_TABLE]
        DECODE_TABLE: // tabla de saltos: 8 entradas x 2 bytes
        dw offset handler_literal dw offset handler_skip_long dw offset handler_skip_short
        dw offset handler_word_fill dw offset handler_byte_fill
        dw offset handler_scanline_fill dw offset handler_backref_rel dw offset handler_backref_abs
        */
    asm cmp bx, 0; asm je handler_literal;
    asm cmp bx, 1; asm je handler_skip_long;
    asm cmp bx, 2; asm je handler_skip_short;
    asm cmp bx, 3; asm je handler_word_fill;
    asm cmp bx, 4; asm je handler_byte_fill;
    asm cmp bx, 5; asm je handler_scanline_fill;
    asm cmp bx, 6; asm je handler_backref_rel;
    asm cmp bx, 7; asm je handler_backref_abs;
    asm jmp _default

    CHECK_DONE:
    asm {
        cmp di, 0xFA00          // ¿buffer lleno?
        jb  LOOP_READ_NEXT_CODE // no (DI < 0xFA00): leer siguiente código
        jmp END_DECODE          // sí: terminar
    }
    handler_literal: //OPCODE 0 — handler_literal
    asm {
        mov cl, byte ptr ds:[si]// leer byte de control
        inc si
        and cx, 0x001F          // aislar bits 4-0 (count-1)
        inc cx                  // count real (1..32)
        REP MOVSB               // memcpy
        jmp CHECK_DONE
    }
    handler_skip_long://OPCODE 1 — handler_skip_long
    asm {
        LODSW
        XCHG AH,AL     //ah high; al low
        and ax, 0x1FFF          // aislar 13 bits (skip-1)
        inc ax
        add di, ax              // avanzar destino
        jmp CHECK_DONE
    }
    handler_skip_short://OPCODE 2 — handler_skip_short
    asm {
        LODSB // leer byte de control
        and ax, 0x001F  // aislar 5 bits — AH ya es 0
        inc ax
        add di, ax
        jmp CHECK_DONE
    }
    handler_word_fill: //OPCODE 3 — handler_word_fill
    asm {
        mov CX, word ptr ds:[si]
        XCHG CH,CL  //CH high byte, CL low byte
        add     si,2
        and     cx, 0x1FFF   // aislar 13 bits
        LODSW   // leer WORD valor
        REP STOSW // escribir WORD
        jmp CHECK_DONE
    }
    handler_byte_fill://OPCODE 4 — handler_byte_fill
    asm {
        LODSW
        XCHG  AL,AH //ah high; al low
        and AX, 0x1FFF          // aislar 13 bits (count-1)
        inc AX                  // count real
        MOV CX,AX               // count
        LODSB                   // leer color
        REP STOSB
        jmp CHECK_DONE
    }
    handler_scanline_fill://OPCODE 5 — handler_scanline_fill
    asm {
        LODSW                  // AH = color, AL = opcode<<5 | count-1
        and  al, 0x1F          // aislar count-1 del primer byte
        inc  al                // count real en AL
        MOV  CL,AL
        XOR  CH,CH             // CX count real
        XCHG AL,AH             // AL = color
        REP STOSB
        jmp CHECK_DONE
    }
    handler_backref_rel:// OPCODE 6 — handler_backref_rel
    asm{
        LODSW
        XCHG AH,AL                 // ah offset h al offset l
        and ax, 0x1FFF             // aislar 13 bits (offset_back-1)
        inc ax                     // offset_back real
        mov bx, di                 // BX = posición destino actual
        sub bx, ax                 // BX = DI - offset_back (fuente)
        mov cl, byte ptr ds:[si]   // leer count-1
        inc si
        and cx, 0x00FF             // aislar byte bajo
        inc cx                     // count real
    }
        backref_rel_loop:
        asm{
            mov al, byte ptr es:[bx]// leer byte del destino
            inc bx
            STOSB                   // escribir en destino es:[di]
            loop backref_rel_loop
        jmp     CHECK_DONE
        }
    handler_backref_abs://OPCODE 7 — handler_backref_abs
    asm {
        mov cl, byte ptr ds:[si]   // leer byte de control
        inc si
        and cx, 0x001F             // aislar bits 4-0 (count-1)
        inc cx                     // count real
        mov bx, word ptr ds:[si]   // BX = offset absoluto (WORD LE)
        add si, 2
    }
        backref_rel_loop1:
        asm{
            mov al, byte ptr es:[bx]// leer byte del destino
            inc bx
            STOSB                   // escribir en destino es:[di]
            loop backref_rel_loop1
            jmp     CHECK_DONE
        }
    _default:
        asm inc si
        asm jmp CHECK_DONE

    END_DECODE://END_DECODE
    asm pop ds; asm pop si; asm pop di;
}
void decode_image_asm0(byte *compressed_data, byte *destination_buffer);


word Ship_X = 1;
word Ship_Y = 47;
int Speed_X = 1;
int Speed_Y = 1;
byte BKG_Ship_RestoreBuffer[512] = {0};

void Restore_BKG_Ship(){
    word offset = (Ship_Y*320) + Ship_X;
    word offset1 = 0;
    byte x,y;

    //Paste restore
    offset = (Ship_Y*320) + Ship_X;
    for (y = 0; y != 20; y++){
        for (x = 0; x != 25; x++){
            Fuzzy_RAM_Buffer0[offset] = BKG_Ship_RestoreBuffer[offset1++];
            offset++;
        }
        offset+=320-25;
    }

    //Now Update ship pos
    Ship_X+=Speed_X;Ship_Y+=Speed_Y;
}



void Paste_BKG_Ship(){
    word offset = (Ship_Y*320) + Ship_X;
    word offset1 = 0;
    byte x,y;

    if (Ship_X+32 == 320) Speed_X*=-1;
    if (Ship_X == 0) Speed_X*=-1;
    if (Ship_Y+32 == 200) Speed_Y*=-1;
    if (Ship_Y == 0) Speed_Y*=-1;
    //Get bkg
    for (y = 0; y != 20; y++){
        for (x = 0; x != 25; x++){
            BKG_Ship_RestoreBuffer[offset1++] = Fuzzy_RAM_Buffer0[offset];
            offset++;
        }
        offset+=320-25;
    }
    //Paste ship
    offset = (Ship_Y*320) + Ship_X;
    for (y = 0; y != 20; y++){
        for (x = 0; x != 25; x++){
            if (Fuzzy_RAM_Buffer0[offset] < 32) Fuzzy_RAM_Buffer0[offset] = 16;
            offset++;
        }
        offset+=320-25;
    }
}


//############
//#   main   #
//############

int main(int argc, char *argv[]){
    char *file_name = argv[1];
    byte debug_load_image = atoi(argv[2]);
    char *resource_name = argv[3];
    byte running = 1;

    if (argc < 3) resource_name[0] = 32;

    Allocate_Memory();

    Init_Game();

    //SET MODE 13h
    asm mov ax,0x0013;asm int 0x10;

    //GET ANI or SPF
    if (!Read_Animation_From_RES(file_name,debug_load_image,resource_name)) {
        Set_Text_Mode();
        printf("Can't find %s in %s\n", resource_name, file_name);
        Fuzzy_Exit();
        return 0;
    }

    //clean vram and paste base frame
    memset(VGA,0,320*200);

    decode_image_asm0(Fuzzy_BKG_Animation_FrameData[0],Fuzzy_RAM_Buffer0);
    Fuzzy_BlitFrame();
    Fuzzy_BKG_Animation_Frame = 1;
    while(!kbhit()){
        if(Fuzzy_BKG_Animation_Frames){
            Wait_Vsync();
            decode_image_asm0(Fuzzy_BKG_Animation_FrameData[Fuzzy_BKG_Animation_Frame],Fuzzy_RAM_Buffer0);
            Paste_BKG_Ship();
            Fuzzy_BlitFrame();
            Restore_BKG_Ship();
            Fuzzy_BKG_Animation_Frame++;
            if (Fuzzy_BKG_Animation_Frame == Fuzzy_BKG_Animation_Frames+1) Fuzzy_BKG_Animation_Frame = 1;
        }
    }

    Set_Text_Mode();
    Fuzzy_Exit();
    return 0;
}
