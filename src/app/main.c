// convert.c : NBT <-> SNBT 单向转换工具
#include "nbtfile.h"
#include "snbtfile.h"
#include "snbt.h"
#include "snbterror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <input> <output> <--to-snbt|--to-nbt> [pretty=0|1]\n", argv[0]);
        return 1;
    }

    const char* input  = argv[1];
    const char* output = argv[2];
    const char* dir    = argv[3];
    int pretty = 0;
    if (argc >= 5) pretty = atoi(argv[4]);

    const nbt_endian endian = NBT_LITTLE_ENDIAN; // 基岩版小端

    if (strcmp(dir, "--to-snbt") == 0) {
        // NBT -> SNBT
        struct NBT_tag* root = fprnbt(input, endian);
        if (!root) {
            fprintf(stderr, "Failed to read NBT: %s\n", snbt_error_string(snbt_get_error()));
            return 1;
        }
        if (!fpwsnbt(output, root, pretty)) {
            fprintf(stderr, "Failed to write SNBT: %s\n", snbt_error_string(snbt_get_error()));
            free_tag(root);
            return 1;
        }
        free_tag(root);
        printf("Converted NBT -> SNBT successfully.\n");
    } else if (strcmp(dir, "--to-nbt") == 0) {
        // SNBT -> NBT
        struct NBT_tag* root = fprsnbt(input);
        if (!root) {
            fprintf(stderr, "Failed to read SNBT: %s\n", snbt_error_string(snbt_get_error()));
            return 1;
        }
        if (!fpwnbt(output, root, endian)) {
            fprintf(stderr, "Failed to write NBT: %s\n", snbt_error_string(snbt_get_error()));
            free_tag(root);
            return 1;
        }
        free_tag(root);
        printf("Converted SNBT -> NBT successfully.\n");
    } else {
        fprintf(stderr, "Invalid direction: %s (use --to-snbt or --to-nbt)\n", dir);
        return 1;
    }

    return 0;
}