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


//PALETTE
byte Fuzzy_Palette[768] = {0};

//BACKGROUND ANIMATION
word Fuzzy_BKG_Animation_Frames;
byte Fuzzy_BKG_Animation_Frame;
word Fuzzy_BKG_Animation_TSize;
word Fuzzy_BKG_Animation_Frame_Size[128];
word Fuzzy_BKG_Animation_Frame_Offsets[128];
byte *Fuzzy_BKG_Animation_Data0;
byte *Fuzzy_BKG_Animation_FrameData[120];

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
    int i = 0; word offset = 0;
    fread(&Fuzzy_BKG_Animation_Frames, 1, 2, in);
    fread(&Fuzzy_BKG_Animation_TSize, 1, 2, in);
    fread(Fuzzy_Palette, 1, 768, in);
    set_palette(Fuzzy_Palette);

    for (i = 0; i != Fuzzy_BKG_Animation_Frames+1; i++){
        Fuzzy_BKG_Animation_Frame_Offsets[i] = offset;
        fread(&Fuzzy_BKG_Animation_Frame_Size[i], 1, 2, in);
        fread(Fuzzy_BKG_Animation_Data0+offset,1,Fuzzy_BKG_Animation_Frame_Size[i],in);
        Fuzzy_BKG_Animation_FrameData[i] = Fuzzy_BKG_Animation_Data0 + offset;
        offset += Fuzzy_BKG_Animation_Frame_Size[i];
    }
}


//Read file from RES
int Read_Animation_From_RES(char *file_name, char *resource_name){
    unsigned short nroffiles;
    char filename[16];      // 12 chars + null terminator
    char cleanname[16];
    unsigned long offset;
    word length;
    word length1;
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

        //IF found file
        if (!strcmp(filename,resource_name)){
            // Read offset and length
            fread(&offset, 4, 1, in);
            fread(&length, 2, 1, in);
            fread(&length1, 2, 1, in);
            fseek(in, offset, SEEK_SET);
            printf("%li %04X\n",offset, length);
            //read resource image
            Read_Animation(in);
            fclose(in);
            return 1;
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
        byte op   = (code >> 5);// & 0x7;
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
                while (cx-- && di + 1 < end) {*di++ = lo;*di++ = hi;}
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
                if (di + cx > end) cx = (int)(end - di);
                memset(di, color,cx);
                di += cx;
                break;
            case 6:   // Back-reference (relative offset)
                ax = (word)((((word)si[0] << 8) | si[1]) & 0x1FFF);
                si += 2;
                offset = (int)ax + 1;
                cx     = (*si++ & 0xFF) + 1;
                pos = (di - dst) - offset;
                while (cx-- && di < end) {*di++ = dst[pos]; pos++;}
                break;
            case 7: //Back-reference (absolute offset)
                cx = (*si & 0x1F) + 1;
                si++;
                bx = (word)(si[0] | ((word)si[1] << 8));
                si += 2;
                while (cx-- && di < end) *di++ = dst[bx++];
                break;
            default:  si++; break;
        }
    }
    return (word)(si - src);
}


//############
//#   main   #
//############

int main(int argc, char *argv[]){
    char *file_name = argv[1];
    char *resource_name = argv[2];
    byte running = 1;

    if (argc < 3) resource_name[0] = 32;

    //ALLOCATE MEMORY
    Fuzzy_BKG_Animation_Data0 = (byte*) calloc(65535,1);
    if (!Fuzzy_BKG_Animation_Data0) {printf("Can't allocate RAM\n");return 1;}

    //SET MODE 13h
    asm mov ax,0x0013;asm int 0x10;

    //GET ANI or SPF
    if (!Read_Animation_From_RES(file_name,resource_name)) {
        asm mov ax,0x0003;asm int 0x10;
        printf("Can't find %s in %s\n", resource_name, file_name);
        free(Fuzzy_BKG_Animation_Data0);
        return 0;
    }

    //clean vram and paste base frame
    memset(VGA,0,320*200);
    Decode_Image(Fuzzy_BKG_Animation_FrameData[0],VGA);
    Fuzzy_BKG_Animation_Frame = 1;

    while(!kbhit());
    Clearkb();

    while(!kbhit()){
        Decode_Image(Fuzzy_BKG_Animation_FrameData[Fuzzy_BKG_Animation_Frame],VGA);
        Wait_Vsync();Wait_Vsync();
        Fuzzy_BKG_Animation_Frame++;
        if (Fuzzy_BKG_Animation_Frame == Fuzzy_BKG_Animation_Frames+1) Fuzzy_BKG_Animation_Frame = 1;
    }


    //EXIT DOS
    free(Fuzzy_BKG_Animation_Data0);
    //SET MODE 03h
    asm mov ax,0x0003;asm int 0x10;

    return 0;
}
