#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fs.h"

#define DISK_SIZE (10 * 1024 * 1024) // 10 MB Disk Image

// Helper to write zeros to pad file
void write_zeros(FILE *fp, size_t count)
{
    uint8_t zero = 0;
    for (size_t i = 0; i < count; i++)
    {
        fwrite(&zero, 1, 1, fp);
    }
}

int main(int argc, char *argv[])
{
    FILE *disk_fp = fopen("disk.img", "wb");
    if (!disk_fp)
    {
        perror("Failed to open disk.img");
        return 1;
    }

    // Initialize the disk with zeros (10MB)
    // This is inefficient but simple for now.
    printf("Initializing 10MB disk image... ");
    uint8_t *disk_buffer = (uint8_t *)calloc(DISK_SIZE, 1);
    fwrite(disk_buffer, 1, DISK_SIZE, disk_fp);
    free(disk_buffer);
    printf("Done.\n");

    // Rewind to start writing structured data
    fseek(disk_fp, 0, SEEK_SET);

    // Write Boot Sector (Sector 0)
    // For now, we use the existing boot.bin as a placeholder MBR.
    // Later, this boot.bin needs to be updated to load Stage 2.
    FILE *boot_fp = fopen("boot.bin", "rb");
    if (!boot_fp)
    {
        printf("WARNING: boot.bin not found. Skipping boot sector.\n");
    }
    else
    {
        printf("Writing boot sector...\n");
        uint8_t boot_buf[512];
        size_t n = fread(boot_buf, 1, 512, boot_fp);
        // Pad with zeros if boot.bin is less than 512 bytes (should not happen for valid MBR)
        if (n < 512)
        {
            memset(boot_buf + n, 0, 512 - n);
        }
        fwrite(boot_buf, 1, 512, disk_fp);
        fclose(boot_fp);
    }

    // Write Stage 2 Bootloader (Sectors 1 to 16)
    FILE *loader_fp = fopen("loader.bin", "rb");
    if (!loader_fp)
    {
        printf("WARNING: loader.bin not found. Reserved space will be zeroed.\n");
    }
    else
    {
        printf("Writing Stage 2 Loader...\n");
        // We have 16 sectors = 16 * 512 = 8192 bytes
        uint8_t *loader_buf = (uint8_t *)calloc(16 * 512, 1);
        fread(loader_buf, 1, 16 * 512, loader_fp);
        
        fseek(disk_fp, 1 * 512, SEEK_SET); // Seek to Sector 1
        fwrite(loader_buf, 1, 16 * 512, disk_fp);
        
        free(loader_buf);
        fclose(loader_fp);
    }

    // Write Superblock (Sector 1 + 16 reserved sectors = Sector 17)
    // We reserve 16 sectors (8KB) for the Stage 2 Bootloader immediately after the MBR.
    uint32_t reserved_sectors = 16;
    uint32_t sb_block_idx = 1 + reserved_sectors; // Sector 17

    printf("Writing Superblock at block %d...\n", sb_block_idx);
    sfs_superblock sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = SIMPLEFS_MAGIC;
    sb.total_blocks = DISK_SIZE / PROJ_BLOCK_SIZE;
    sb.inode_bitmap_block = sb_block_idx + 1; // Sector 18
    sb.inode_table_block = sb_block_idx + 2;  // Sector 19
    sb.data_block_start = sb_block_idx + 10;  // Sector 27
    sb.num_inodes = (sb.data_block_start - sb.inode_table_block) * (PROJ_BLOCK_SIZE / sizeof(sfs_inode));

    fseek(disk_fp, sb_block_idx * PROJ_BLOCK_SIZE, SEEK_SET);
    fwrite(&sb, 1, sizeof(sb), disk_fp);

    // Write Root Inode (File: "kernel.bin")
    // We place it at index 0 of the inode table.
    // Real FS would have dynamic allocation, here we hardcode for bootstrapping.
    printf("Writing Kernel Inode...\n");

    FILE *kernel_fp = fopen("kernel.bin", "rb");
    if (!kernel_fp)
    {
        printf("WARNING: kernel.bin not found. Kernel will not be written.\n");
    }
    else
    {
        // Get kernel size
        fseek(kernel_fp, 0, SEEK_END);
        uint32_t kernel_size = ftell(kernel_fp);
        fseek(kernel_fp, 0, SEEK_SET);
        
        printf("Kernel size: %d bytes\n", kernel_size);

        // Create Inode
        sfs_inode kernel_inode;
        memset(&kernel_inode, 0, sizeof(kernel_inode));
        kernel_inode.used = 1;
        strcpy(kernel_inode.filename, "kernel.bin");
        kernel_inode.size = kernel_size;

        // Calculate and Assign Data Blocks
        // Assuming kernel fits in contiguous blocks starting at data_block_start
        uint32_t needed_blocks = (kernel_size + PROJ_BLOCK_SIZE - 1) / PROJ_BLOCK_SIZE;
        if (needed_blocks > 64)
        {
            printf("WARNING: Kernel too big (%d blocks) for direct blocks! Only first 64 blocks will be indexed.\n", needed_blocks);
            needed_blocks = 64; // Truncate for now as per simple design
        }

        for (uint32_t i = 0; i < needed_blocks; i++)
        {
            kernel_inode.blocks[i] = sb.data_block_start + i;
        }

        // Write Inode to Inode Table (Sector 19)
        // Position: Start of Sector 19 (first inode)
        fseek(disk_fp, sb.inode_table_block * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(&kernel_inode, 1, sizeof(kernel_inode), disk_fp);

        // Write Kernel Data to Data Blocks
        printf("Writing Kernel Data...\n");
        uint8_t *kernel_data = (uint8_t *)malloc(kernel_size);
        fread(kernel_data, 1, kernel_size, kernel_fp);

        fseek(disk_fp, sb.data_block_start * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(kernel_data, 1, kernel_size, disk_fp);

        free(kernel_data);
        fclose(kernel_fp);
    }

    fclose(disk_fp);
    printf("Successfully created disk.img!\n");
    return 0;
}
