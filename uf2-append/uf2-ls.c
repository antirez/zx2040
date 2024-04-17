/* Copyright (C) 2024 Salvatore Sanfilippo -- All Rights Reserved
 * This software is relased under the MIT license.
 *
 * This utility shows the blocks contained inside an UF2 file. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h> // strtoul()
#include <string.h>

struct UF2_Block {
    // 32 byte header
    uint32_t magicStart0;
    uint32_t magicStart1;
    uint32_t flags;
    uint32_t targetAddr;
    uint32_t payloadSize;
    uint32_t blockNo;
    uint32_t numBlocks;
    uint32_t familyID; // In all modern UF2 files this is familyID, not size.
    uint8_t data[476];
    uint32_t magicEnd;
};

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr,"Usage: %s file.uf2\n", argv[0]);
        return 1;
    }

    // Open all the needed files.
    FILE *in_fp = fopen(argv[1],"r");
    if (in_fp == NULL) {
        perror("Opening input file");
        return 1;
    }

    struct UF2_Block block;
    while (fread(&block,sizeof(block),1,in_fp) == 1) {
        printf("Block: %u/%u ",  block.blockNo+1, block.numBlocks);
        printf("Family: 0x%08x ", block.familyID);
        printf("Flags: 0x%08x ", block.flags);
        printf("%u bytes@", block.payloadSize);
        printf("0x%08x\n", block.targetAddr);
    }
    return 0;
}
