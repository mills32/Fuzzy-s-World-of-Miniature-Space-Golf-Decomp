#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;

#define IMG_WIDTH  320
#define IMG_HEIGHT 200
#define IMG_PIXELS (IMG_WIDTH * IMG_HEIGHT)  /* 64000 */
#define LZW_MAX_CODES  4096
#define LZW_HASH_SIZE  16411   /* prime larger than LZW_MAX_CODES * 4 */
#define LZW_UNDEFINED  0xFFFFu


typedef struct {
    FILE    *f;

    /* Sub-block output buffer (GIF max 255 bytes/block) */
    byte  block[255];
    int      block_pos;
    dword bit_buf;
    int      bit_count;

    /* Open-addressed hash table: (prefix, suffix) -> assigned code */
    word ht_prefix[LZW_HASH_SIZE];
    byte ht_suffix[LZW_HASH_SIZE];
    word ht_code  [LZW_HASH_SIZE];

    int num_codes;
    int code_size;
    int clear_code;   /* = 256 */
    int eoi_code;     /* = 257 */
} LZW;

void lzw_flush_block(LZW *lzw){
    if (lzw->block_pos > 0) {
        fputc(lzw->block_pos, lzw->f);
        fwrite(lzw->block, 1, (size_t)lzw->block_pos, lzw->f);
        lzw->block_pos = 0;
    }
}

void lzw_emit_bits(LZW *lzw, dword code, int bits){
    lzw->bit_buf   |= code << lzw->bit_count;
    lzw->bit_count += bits;
    while (lzw->bit_count >= 8) {
        lzw->block[lzw->block_pos++] = (byte)(lzw->bit_buf & 0xFF);
        lzw->bit_buf   >>= 8;
        lzw->bit_count  -= 8;
        if (lzw->block_pos == 255) lzw_flush_block(lzw);
    }
}

void lzw_flush_bits(LZW *lzw){
    if (lzw->bit_count > 0) {
        lzw->block[lzw->block_pos++] = (byte)(lzw->bit_buf & 0xFF);
        if (lzw->block_pos == 255) lzw_flush_block(lzw);
    }
    lzw_flush_block(lzw);
    fputc(0, lzw->f);   /* GIF sub-block terminator */
}

void lzw_clear_table(LZW *lzw){
    memset(lzw->ht_code, 0xFF, sizeof(lzw->ht_code));
    lzw->num_codes = lzw->eoi_code + 1;   /* 258 */
    lzw->code_size = 9;                    /* start at min_code_size+1 */
}

/* Returns slot index if found (>=0), or -(empty_slot+1) if not found */
int lzw_find(LZW *lzw, word prefix, byte suffix){
    dword h = (((dword)prefix << 8) | suffix) * 0x9E3779B1u;
    int slot = (int)((h >> 14) % (dword)LZW_HASH_SIZE);
    int step = 1;
    while (lzw->ht_code[slot] != LZW_UNDEFINED) {
        if (lzw->ht_prefix[slot] == prefix && lzw->ht_suffix[slot] == suffix)
            return slot;
        slot = (slot + step) % LZW_HASH_SIZE;
        step += 2;
    }
    return -(slot + 1);
}

void lzw_insert(LZW *lzw, int empty_slot, word prefix, byte suffix){
    lzw->ht_prefix[empty_slot] = prefix;
    lzw->ht_suffix[empty_slot] = suffix;
    lzw->ht_code  [empty_slot] = (word)lzw->num_codes++;
    if (lzw->num_codes > (1 << lzw->code_size) && lzw->code_size < 12)
        lzw->code_size++;
}

static void lzw_compress(FILE *f, const byte *pixels, int n){
    LZW lzw;
    memset(&lzw, 0, sizeof(lzw));
    lzw.f          = f;
    lzw.clear_code = 256;
    lzw.eoi_code   = 257;

    fputc(8, f);   /* minimum LZW code size (always 8 for 256-color GIFs) */

    lzw_clear_table(&lzw);
    lzw_emit_bits(&lzw, (dword)lzw.clear_code, lzw.code_size);

    word prefix = pixels[0];

    for (int i = 1; i < n; i++) {
        byte suffix = pixels[i];
        int     slot   = lzw_find(&lzw, prefix, suffix);

        if (slot >= 0) {
            prefix = lzw.ht_code[slot];   /* string exists: extend it */
        } else {
            lzw_emit_bits(&lzw, prefix, lzw.code_size);

            if (lzw.num_codes < LZW_MAX_CODES) {
                lzw_insert(&lzw, -(slot + 1), prefix, suffix);
            } else {
                /* Code table full: emit clear and reset */
                lzw_emit_bits(&lzw, (dword)lzw.clear_code, lzw.code_size);
                lzw_clear_table(&lzw);
            }
            prefix = suffix;
        }
    }

    lzw_emit_bits(&lzw, prefix, lzw.code_size);
    lzw_emit_bits(&lzw, (dword)lzw.eoi_code, lzw.code_size);
    lzw_flush_bits(&lzw);
}

/* ===================================================================
 * GIF Encoder — public API
 * =================================================================== */

typedef struct {
    FILE    *f;
    int      width;
    int      height;
    int      num_frames;
    int      frames_written;
    word delay_cs;
} GifEncoder;

static void w8 (FILE *f, byte  v) { fputc(v, f); }
static void w16(FILE *f, word v) { fputc(v & 0xFF, f); fputc(v >> 8, f); }

/*
 * gif_create — open file, write GIF89a header and global color table.
 *
 *  path           output .gif file path
 *  width/height   canvas dimensions in pixels
 *  vga_palette    256 * 3 bytes, RGB, each channel 0–63 (6-bit VGA format)
 *  num_frames     total frame count (pass 1 for still image)
 *  frame_delay_cs delay between frames in centiseconds (e.g. 4 = 25fps)
 *                 ignored for single-frame images
 *
 * Returns NULL on failure.
 */
GifEncoder *gif_create(char *path,int width, int height, const byte *vga_palette, int num_frames,word frame_delay_cs){
    int i = 0;
    GifEncoder *enc = (GifEncoder *)calloc(1, sizeof(GifEncoder));
    if (!enc) return NULL;

    enc->f              = fopen(path, "wb");
    enc->width          = width;
    enc->height         = height;
    enc->num_frames     = num_frames;
    enc->frames_written = 0;
    enc->delay_cs       = frame_delay_cs;

    if (!enc->f) { perror("gif_create"); free(enc); return NULL; }


    /* --- GIF89a signature --- */
    fwrite("GIF89a", 1, 6, enc->f);

    /* --- Logical Screen Descriptor (7 bytes) --- */
    w16(enc->f, (word)width);
    w16(enc->f, (word)height);
    w8 (enc->f, 0xF7);   /* flags: GCT present(1) | color-res=8(111) | not sorted(0) | GCT-size=8(111) */
    w8 (enc->f, 0x00);   /* background color index */
    w8 (enc->f, 0x00);   /* pixel aspect ratio (0 = square) */

    // Global Color Table:
    for (i = 0; i < 256; i++) {
        /* Scale VGA 6-bit (0-63) -> 8-bit (0-252) by multiplying by 4 */
        w8(enc->f, (byte)(vga_palette[i*3 + 0] * 4));   /* R */
        w8(enc->f, (byte)(vga_palette[i*3 + 1] * 4));   /* G */
        w8(enc->f, (byte)(vga_palette[i*3 + 2] * 4));   /* B */
    }

    /* --- Netscape Application Extension (animation looping) --- */
    w8(enc->f, 0x21); w8(enc->f, 0xFF); w8(enc->f, 0x0B);
    fwrite("NETSCAPE2.0", 1, 11, enc->f);
    w8(enc->f, 0x03); w8(enc->f, 0x01);
    w16(enc->f, 0x0000);   /* loop count: 0 = infinite */
    w8(enc->f, 0x00);      /* sub-block terminator */

    return enc;
}

/*
 * gif_add_frame — encode and append one frame to the GIF.
 *
 *  pixels   width*height bytes, row-major top-to-bottom,
 *            values are palette indices 0–255.
 *
 * Returns 0 on success, -1 on error.
 */
int gif_add_frame(GifEncoder *enc, const byte *pixels){
    FILE *f;
    if (!enc || !enc->f) return -1;
    f = enc->f;

    /* --- Graphic Control Extension (sets delay and disposal) --- */
    w8 (f, 0x21);        /* extension introducer */
    w8 (f, 0xF9);        /* graphic control label */
    w8 (f, 0x04);        /* block size (always 4) */
    w8 (f, 0x00);        /* packed: disposal=none, no user input, no transparency */
    w16(f, enc->delay_cs);
    w8 (f, 0x00);        /* transparent color index (unused) */
    w8 (f, 0x00);        /* block terminator */

    /* --- Image Descriptor (10 bytes) --- */
    w8 (f, 0x2C);                      /* image separator */
    w16(f, 0x0000);                    /* left offset */
    w16(f, 0x0000);                    /* top offset */
    w16(f, (word)enc->width);
    w16(f, (word)enc->height);
    w8 (f, 0x00);                      /* packed: no local palette, not interlaced */

    /* --- LZW compressed pixel data --- */
    lzw_compress(f, pixels, enc->width * enc->height);

    enc->frames_written++;
    return 0;
}

/*
 * gif_close — write GIF trailer and close the file.
 * Always call this even on error to avoid file leaks.
 */
void gif_close(GifEncoder *enc){
    if (!enc) return;
    if (enc->f) {
        fputc(0x3B, enc->f);   /* GIF trailer */
        fclose(enc->f);
    }
    free(enc);
}



/* ---------------------------------------------------------------
 * Decode SPF / ANI from Fuzzy's World
 * --------------------------------------------------------------- */

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
            if (color) memset(di,color,bx);
            si += 3; di += bx;
            break;
        case 5:   // Scanline fill: fixed color, short count
            color = si[1];
            cx    = (si[0] & 0x1F) + 1;
            si+=2;
            if (di + cx > end) cx = (int)(end - di);
            if (color) memset(di, color,cx);
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


/* ---------------------------------------------------------------
 * main
 * --------------------------------------------------------------- */
void creategif(FILE *fin, char *outname){
    byte hdr[4];
    word total_size = 0;
    word n_frames = 0;
    byte palette[768];
    byte sz_buf[2];
    word comp_size;
    byte *comp_data;
    word read_bytes;
    byte *pixels;
    word consumed;
    GifEncoder *GIF_enc;
    int i;

    //Prepare RAM
    comp_data = (byte *)malloc(65535);
    pixels = (byte *)calloc(1,320*200);
    fread(&n_frames, 1, 2, fin);
    n_frames+=1;
    fread(&total_size, 1, 2, fin);
    fread(palette, 1, 768, fin);

    //Create GIF
    GIF_enc = gif_create(outname,320,200, palette, n_frames, 7);
    if (!GIF_enc) return;

    printf("frames %i\n",n_frames);

    for (i = 0; i != n_frames; i++){
        fread(sz_buf, 1, 2, fin);
        comp_size = (word)(sz_buf[0] | ((word)sz_buf[1] << 8));
        read_bytes = fread(comp_data, 1, comp_size, fin);
        Decode_Image(comp_data, read_bytes, pixels);
        gif_add_frame(GIF_enc, pixels);
    }

    gif_close(GIF_enc);
    free(comp_data);
    free(pixels);
}

int main(int argc, char *argv[]){
    FILE *in;
    unsigned short nroffiles;
    char filename[16];      // 12 chars + null terminator
    char cleanname[16];
    unsigned long offset;
    unsigned long length;
    unsigned char byte;
    int x, y,i;
    char *resource_name = argv[2];

    if (argc < 3) return 1;

    in = fopen(argv[1], "rb");
    if (!in) {printf("Can't find %s\n", argv[1]);return 1;}

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

        //If found file
        if (!strcmp(filename,resource_name)){
            // Read offset and length
            fread(&offset, 4, 1, in);
            fread(&length, 4, 1, in);
            fseek(in, offset, SEEK_SET);
            printf("FOUND FILE: %s  offset: %08X  length: %08X\n",cleanname, offset, length);
            cleanname[9] = 'G'; cleanname[10] = 'I'; cleanname[11] = 'F'; cleanname[12] = '\0';
            creategif(in,cleanname);
            break;
        }
    }

    fclose(in);
    return 0;
}
