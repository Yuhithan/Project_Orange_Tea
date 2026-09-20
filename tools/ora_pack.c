#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ora.h"

static long file_size(FILE *file)
{
    long end;
    if (fseek(file, 0, SEEK_END) != 0) return -1;
    end = ftell(file);
    if (end < 0 || fseek(file, 0, SEEK_SET) != 0) return -1;
    return end;
}

static int copy_file(FILE *input, FILE *output, long length)
{
    unsigned char buffer[4096];
    while (length > 0) {
        size_t chunk = length > (long)sizeof(buffer) ? sizeof(buffer) : (size_t)length;
        if (fread(buffer, 1, chunk, input) != chunk || fwrite(buffer, 1, chunk, output) != chunk) return 0;
        length -= (long)chunk;
    }
    return 1;
}

int main(int argc, char **argv)
{
    FILE *text;
    FILE *data;
    FILE *output;
    struct ora_header header;
    long text_size;
    long data_size;

    if (argc != 4) {
        fprintf(stderr, "usage: ora_pack TEXT DATA OUTPUT\n");
        return 2;
    }
    text = fopen(argv[1], "rb");
    data = fopen(argv[2], "rb");
    output = fopen(argv[3], "wb");
    if (!text || !data || !output) {
        fprintf(stderr, "ora_pack: cannot open input or output\n");
        return 1;
    }
    text_size = file_size(text);
    data_size = file_size(data);
    if (text_size < 1 || data_size < 0 || text_size > 0xffffffffL || data_size > 0xffffffffL) {
        fprintf(stderr, "ora_pack: invalid segment size\n");
        return 1;
    }
    memset(&header, 0, sizeof(header));
    header.magic = ORA_MAGIC;
    header.version = ORA_VERSION;
    header.architecture = ORA_ARCH_X86_64;
    header.header_size = ORA_HEADER_SIZE;
    header.entry_point = 0;
    header.text_size = (uint64_t)text_size;
    header.data_size = (uint64_t)data_size;
    header.alignment = 16;
    if (fwrite(&header, 1, sizeof(header), output) != sizeof(header) ||
        !copy_file(text, output, text_size) || !copy_file(data, output, data_size)) {
        fprintf(stderr, "ora_pack: write failed\n");
        return 1;
    }
    fclose(text);
    fclose(data);
    fclose(output);
    return 0;
}