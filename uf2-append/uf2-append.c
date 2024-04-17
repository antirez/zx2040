/* Copyright (C) 2024 Salvatore Sanfilippo -- All Rights Reserved
 * This software is relased under the MIT license.
 *
 * This utility takes four arguments:
 * two files and a offset: file1.uf2 file2.bin output.uf2 <offset>
 * The offset can be hex if it starts with '0x'
 * The file1.uf2 is rewritten into output.uf2, adding blocks in
 * order to also flash file2.bin at flash address offset.
 *
 * We use this in order to concat our Z80 games to the emulator
 * executable, but actually this utility can be used by any Pico
 * program that needs to append some data. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h> // strtoul()
#include <string.h>

#define UF2_FLAG_FID_PRESENT 0x00002000 // Family ID preset (not file size).
#define DEBUG 1

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

// Stictly ANSI-C way to get file size.
size_t get_file_size(const char *filename) {
    FILE *fp = fopen(filename,"r");
    if (fp == NULL) return 0;
    fseek(fp,0,SEEK_END);
    size_t file_len = ftell(fp);
    fclose(fp);
    return file_len;
}

#define BLOCK_SIZE 256 // As suggested by UF2, and also the size used by
                       // RP2040 SDK.

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr,"Usage: %s input.uf2 data.bin 0x.... output.uf2\n"
                       "See README.md file for more information.\n",
                       argv[0]);
        return 1;
    }

    // Make arguments more explicit.
    const char *input_uf2_filename = argv[1];
    const char *input_data_filename = argv[2];
    const char *offset_as_string = argv[3];
    const char *output_uf2_filename = argv[4];

    // Compute the number of blocks required for the binary file
    // to append. We will use fixed block data size of 256 bytes
    // for the blocks we will add.
    size_t bin_file_size = get_file_size(input_data_filename);
    if (bin_file_size == 0) {
        printf("Error accessing data file\n");
        return 1;
    }

    // Parse the offset provided by the user.
    uint32_t data_offset = strtoul(offset_as_string,NULL,0);
    if (data_offset == 0 || data_offset & 255) {
        printf("ERROR: Invalid data address: %s\n", offset_as_string);
        printf("It must be 256 bytes aligned and can't be zero.");
        return 1;
    }

    // Open all the needed files.
    FILE *in_fp = fopen(input_uf2_filename,"r");
    FILE *data_fp = fopen(input_data_filename,"r");
    FILE *out_fp = fopen(output_uf2_filename,"w");
    if (in_fp == NULL) perror("Opening input file");
    if (data_fp == NULL) perror("Opening data file");
    if (out_fp == NULL) perror("Opening output file");
    if (in_fp == NULL || data_fp == NULL || out_fp == NULL) return 1;

    uint32_t total_flashed = 0; // Keep track of UF2 total write.
    struct UF2_Block block;

    // Pre-scan the file. We need to find what is the maxium address
    // written to compute how many padding blocks we require between
    // the orginal UF2 blocks and our data blocks at the target address.
    uint32_t targetAddrMax = 0;
    while (fread(&block,sizeof(block),1,in_fp) == 1) {
        if (targetAddrMax < block.targetAddr)
            targetAddrMax = block.targetAddr;
    }
    fseek(in_fp,0,SEEK_SET); // Rewind.

    // If the address the user provided is less than the greatest address
    // the initial UF2 sets, maybe there is some issue.
    if (targetAddrMax+BLOCK_SIZE > data_offset) {
        printf("********************************************************\n");
        printf("* ERROR: UF2 block with address+block_size > data offset.\n");
        printf("*        Block max target is 0x%08x.\n", targetAddrMax);
        printf("********************************************************\n");
        return 1;
    }

    // Compute additional blocks.
    size_t additional_blocks = (bin_file_size+BLOCK_SIZE-1)/BLOCK_SIZE;
    printf("%zu bytes data file: appending %zu blocks to original UF2\n",
        bin_file_size, additional_blocks);

    // Comput padding blocks
    size_t padding_blocks = (data_offset - (targetAddrMax+BLOCK_SIZE)) / 256;
    if (padding_blocks) {
        printf("%zu padding blocks needed from %08x to %08x",
            padding_blocks, targetAddrMax+BLOCK_SIZE, data_offset);
        printf("RP2040 UF2 flasher does not like holes\n");
        additional_blocks += padding_blocks;
        printf("Total additional blocks: %zu\n", additional_blocks);
    }

    // Read input file blocks and copy them as they are
    // just changing the total number of blocks in the output file.
    uint32_t familyID = 0;
    while (fread(&block,sizeof(block),1,in_fp) == 1) {
        if (familyID == 0) {
            familyID = block.familyID;
            printf("Family ID: 0x%08x\n", familyID);
            printf("Flags    : 0x%08x\n", block.flags);
            printf("Target   : 0x%08x\n", block.targetAddr);
            printf("Size     : %u\n", block.payloadSize);
        }
        if (targetAddrMax < block.targetAddr)
            targetAddrMax = block.targetAddr;

        // Adjust block numbers to account for the blocks
        // we are going to append.
        block.numBlocks += additional_blocks;

        if (DEBUG)
            printf("Copying block %u/%u targeting %08x, %u bytes\n",
                block.blockNo+1, block.numBlocks, block.targetAddr,
                block.payloadSize);

        // Write block to target file.
        if (fwrite(&block,sizeof(block),1,out_fp) != 1) {
            perror("Write error");
            return 1;
        }
        total_flashed += block.payloadSize;
    }

    // Sanity check
    if (block.numBlocks-additional_blocks != block.blockNo+1) {
        printf("*** WARNING: input UF2 total block numbers mismatch.\n");
        printf("*** Corrupted input UF2 file?\n");
        printf("\nPROGRAM STOPPED.\n");
        return 1;
    }

    // Now write all the new appended blocks.
    while(bin_file_size != 0) {
        size_t blen = bin_file_size >= BLOCK_SIZE ? BLOCK_SIZE : bin_file_size;

        // Change the block content as needed.
        // We reuse the latest block from the original UF2 file
        // with the total number of blocks already changed and
        // the device family already set correctly.

        // Fix target address.
        block.targetAddr += BLOCK_SIZE;

        // Increment block number.
        block.blockNo++;

        // The payload size should already be ok but we set it anyway.
        block.payloadSize = BLOCK_SIZE;

        // Set data section.
        memset(block.data,0,sizeof(block.data));

        // If this is not a padding block, load data
        if (block.targetAddr >= data_offset) {
            if (fread(block.data,blen,1,data_fp) != 1) {
                perror("Reading from data file");
                exit(1);
            }
            bin_file_size -= blen; // Consume length of data file.
        }

        if (DEBUG)
            printf("Appending %s block %u/%u targeting %08x, %u bytes\n",
                block.targetAddr >= data_offset ? "data" : "padding",
                block.blockNo+1, block.numBlocks, block.targetAddr,
                block.payloadSize);

        // Write block to output file.
        if (fwrite(&block,sizeof(block),1,out_fp) != 1) {
            perror("Write error");
            return 1;
        }
        total_flashed += block.payloadSize;
    }

    // Sanity check
    if (block.numBlocks != block.blockNo+1) {
        printf("*** WARNING: output UF2 total block numbers mismatch.\n");
        printf("*** Corrupted input UF2 file?\n");
        return 1;
    }

    fclose(in_fp);
    fclose(data_fp);
    fclose(out_fp);

    printf("\nDONE:\n");
    printf("%u UF2 total blocks\n", block.numBlocks);
    printf("The generated UF2 file will flash %u bytes in total\n",
        total_flashed);
    return 0;
}
