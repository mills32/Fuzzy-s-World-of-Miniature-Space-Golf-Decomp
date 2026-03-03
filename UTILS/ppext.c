#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){
    FILE *in, *out;
    unsigned short nroffiles;
    char filename[16];      // 12 chars + null terminator
    char cleanname[16];
    unsigned long offset;
    unsigned long length;
    unsigned char byte;
    int x, y;
    unsigned char read_buffer[256*1024];
    printf("Pixel Painters Games Extracter by Frenkel Smeijers\n");

    if (argc < 2) {
        printf("Usage: ppext [resource file]\n");
        printf("Example: ppext enoid.res\n");
        return 1;
    }

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

        // Read offset and length
        fread(&offset, 4, 1, in);
        fread(&length, 4, 1, in);

        out = fopen(cleanname, "wb");
        if (!out) {
            printf("Failed to create %s\n", cleanname);
            continue;
        }

        // Copy file contents
        fseek(in, offset, SEEK_SET);
        fread(read_buffer, 1, length,in);
        fwrite(read_buffer, 1, length, out);

        fclose(out);
        printf("%s %i written\n", cleanname, length);
    }

    fclose(in);
    return 0;
}
