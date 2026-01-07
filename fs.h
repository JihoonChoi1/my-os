#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include <stdint.h>

/*
 * [SimpleFS Design Choice]
 * We are designing a very simple, flat file system.
 * To keep the bootloader simple, we will store filenames directly in the Inode.
 * This is similar to a "Flat Filesystem" where all files are in the root directory.
 */

// 1. Magic Number
// This is a signature. If the first few bytes of the disk match this,
// We know it's formatted with SimpleFS.
#define SIMPLEFS_MAGIC 0x12345678

// 2. Block Size
// We align with the standard sector size (512 bytes) for simplicity.
#define PROJ_BLOCK_SIZE 512

// 3. Filename Length
// Maximum 32 characters for a filename.
#define FILENAME_MAX_LEN 32

/*
 * [Superblock]
 * Contains the global metadata of the file system.
 * It usually resides at the very beginning of the partition (or after the bootblock).
 */
typedef struct {
    uint32_t magic;             // Must confirm to SIMPLEFS_MAGIC
    uint32_t total_blocks;      // Total size of the disk (in blocks)
    uint32_t inode_bitmap_block;// Which block contains the inode usage bitmap?
    uint32_t inode_table_block; // Which block does the inode table start?
    uint32_t data_block_start;  // Which block does the data area start?
    uint32_t num_inodes;        // How many inodes (files) can we store?
    uint8_t  padding[488];      // Pad to 512 bytes (Block Size)
} sfs_superblock;

/*
 * [Inode] (Index Node)
 * Represents a single file.
 * In a real FS, filenames are in "Directory Entries", but for simplicity,
 * we store the filename directly here.
 */
typedef struct {
    uint8_t  used;                    // 1 if this inode is in use, 0 if free
    char     filename[FILENAME_MAX_LEN]; // Name of the file (e.g., "kernel.bin")
    uint32_t size;                    // Size of the file in bytes
    uint32_t blocks[12];              // Direct pointers to data blocks
                                      // (Max file size = 12 * 512 = 6KB for now)
                                      // If we need bigger files, we can increase this
                                      // or use indirect pointers.
    uint8_t  padding[3];              // To align structure if needed (Total ~60 bytes)
                                      // Actual struct size depends on compiler alignment
} sfs_inode;

// Note: sfs_inode is smaller than 512 bytes.
// We can fit multiple inodes in one block.
// 512 / sizeof(sfs_inode) -> roughly 8 inodes per block.

#endif
