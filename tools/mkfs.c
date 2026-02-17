#include <stdio.h>  
#include <stdlib.h> 
#include <string.h> 
#include <stdint.h> 
#include "fs.h"     // The header file containing our custom file system structures (sfs_inode, sfs_superblock).

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

    uint32_t next_free_block = sb.data_block_start;

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
            kernel_inode.blocks[i] = next_free_block + i;
        }

        // Write Inode to Inode Table (Sector 19)
        // Position: Start of Sector 19 (first inode)
        fseek(disk_fp, sb.inode_table_block * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(&kernel_inode, 1, sizeof(kernel_inode), disk_fp);

        // Write Kernel Data to Data Blocks
        printf("Writing Kernel Data...\n");
        uint8_t *kernel_data = (uint8_t *)malloc(kernel_size);
        fread(kernel_data, 1, kernel_size, kernel_fp);

        fseek(disk_fp, next_free_block * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(kernel_data, 1, kernel_size, disk_fp);

        next_free_block += needed_blocks;
        free(kernel_data);
        fclose(kernel_fp);
    }

    // Write User Program Inode (File: "hello.elf")
    printf("Writing hello.elf Inode...\n");
    FILE *prog_fp = fopen("programs/hello.elf", "rb");
    if (!prog_fp)
    {
        printf("WARNING: programs/hello.elf not found. Skipping.\n");
    }
    else
    {
        fseek(prog_fp, 0, SEEK_END);
        uint32_t prog_size = ftell(prog_fp);
        fseek(prog_fp, 0, SEEK_SET);
        
        printf("hello.elf size: %d bytes\n", prog_size);

        sfs_inode prog_inode;
        memset(&prog_inode, 0, sizeof(prog_inode));
        prog_inode.used = 1;
        strcpy(prog_inode.filename, "hello.elf");
        prog_inode.size = prog_size;
        
        uint32_t needed_blocks = (prog_size + PROJ_BLOCK_SIZE - 1) / PROJ_BLOCK_SIZE;
        
        for (uint32_t i = 0; i < needed_blocks; i++)
        {
            prog_inode.blocks[i] = next_free_block + i;
        }

        // Write Inode to Inode Table (Sector 19)
        // Index 1 (Second inode)
        fseek(disk_fp, sb.inode_table_block * PROJ_BLOCK_SIZE + sizeof(sfs_inode), SEEK_SET);
        fwrite(&prog_inode, 1, sizeof(prog_inode), disk_fp);

        // Write Data
        printf("Writing hello.elf Data...\n");
        uint8_t *prog_data = (uint8_t *)malloc(prog_size);
        fread(prog_data, 1, prog_size, prog_fp);

        fseek(disk_fp, next_free_block * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(prog_data, 1, prog_size, disk_fp);

        free(prog_data);
        next_free_block += needed_blocks;
        fclose(prog_fp);
    }
    
    // Write Shell Program Inode (File: "shell.elf")
    printf("Writing shell.elf Inode...\n");
    FILE *shell_fp = fopen("programs/shell.elf", "rb");
    if (!shell_fp)
    {
        printf("WARNING: programs/shell.elf not found. Skipping.\n");
    }
    else
    {
        fseek(shell_fp, 0, SEEK_END);
        uint32_t shell_size = ftell(shell_fp);
        fseek(shell_fp, 0, SEEK_SET);
        
        printf("shell.elf size: %d bytes\n", shell_size);

        sfs_inode shell_inode;
        memset(&shell_inode, 0, sizeof(shell_inode));
        shell_inode.used = 1;
        strcpy(shell_inode.filename, "shell.elf");
        shell_inode.size = shell_size;
        
        uint32_t needed_blocks = (shell_size + PROJ_BLOCK_SIZE - 1) / PROJ_BLOCK_SIZE;
        
        for (uint32_t i = 0; i < needed_blocks; i++)
        {
            shell_inode.blocks[i] = next_free_block + i;
        }

        // Write Inode to Inode Table (Sector 19)
        // Index 2 (Third inode)
        fseek(disk_fp, sb.inode_table_block * PROJ_BLOCK_SIZE + 2 * sizeof(sfs_inode), SEEK_SET);
        fwrite(&shell_inode, 1, sizeof(shell_inode), disk_fp);

        // Write Data
        printf("Writing shell.elf Data...\n");
        uint8_t *shell_data = (uint8_t *)malloc(shell_size);
        fread(shell_data, 1, shell_size, shell_fp);

        fseek(disk_fp, next_free_block * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(shell_data, 1, shell_size, disk_fp);

        free(shell_data);
        next_free_block += needed_blocks;
        fclose(shell_fp);
    }
    
    // Write fork_cow.elf Program Inode
    printf("Writing fork_cow.elf Inode...\n");
    FILE *cow_fp = fopen("programs/fork_cow.elf", "rb");
    if (!cow_fp) {
        printf("WARNING: programs/fork_cow.elf not found. Skipping.\n");
    } else {
        fseek(cow_fp, 0, SEEK_END);
        uint32_t cow_size = ftell(cow_fp);
        fseek(cow_fp, 0, SEEK_SET);
        
        printf("fork_cow.elf size: %d bytes\n", cow_size);

        sfs_inode cow_inode;
        memset(&cow_inode, 0, sizeof(cow_inode));
        cow_inode.used = 1;
        strcpy(cow_inode.filename, "fork_cow.elf");
        cow_inode.size = cow_size;
        
        uint32_t needed_blocks = (cow_size + PROJ_BLOCK_SIZE - 1) / PROJ_BLOCK_SIZE;
        
        for (uint32_t i = 0; i < needed_blocks; i++) {
            cow_inode.blocks[i] = next_free_block + i;
        }

        // Write Inode to Inode Table (Sector 19)
        // Index 3 (Fourth inode)
        fseek(disk_fp, sb.inode_table_block * PROJ_BLOCK_SIZE + 3 * sizeof(sfs_inode), SEEK_SET);
        fwrite(&cow_inode, 1, sizeof(cow_inode), disk_fp);

        // Write Data
        printf("Writing fork_cow.elf Data...\n");
        uint8_t *cow_data = (uint8_t *)malloc(cow_size);
        fread(cow_data, 1, cow_size, cow_fp);

        fseek(disk_fp, next_free_block * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(cow_data, 1, cow_size, disk_fp);

        free(cow_data);
        next_free_block += needed_blocks;
        fclose(cow_fp);
    }
    
    // Write thread_test.elf Program Inode
    printf("Writing thread_test.elf Inode...\n");
    FILE *thread_fp = fopen("programs/thread_test.elf", "rb");
    if (!thread_fp) {
        printf("WARNING: programs/thread_test.elf not found. Skipping.\n");
    } else {
        fseek(thread_fp, 0, SEEK_END);
        uint32_t thread_size = ftell(thread_fp);
        fseek(thread_fp, 0, SEEK_SET);
        
        printf("thread_test.elf size: %d bytes\n", thread_size);

        sfs_inode thread_inode;
        memset(&thread_inode, 0, sizeof(thread_inode));
        thread_inode.used = 1;
        strcpy(thread_inode.filename, "thread_test.elf");
        thread_inode.size = thread_size;
        
        uint32_t needed_blocks = (thread_size + PROJ_BLOCK_SIZE - 1) / PROJ_BLOCK_SIZE;
        
        for (uint32_t i = 0; i < needed_blocks; i++) {
            thread_inode.blocks[i] = next_free_block + i;
        }

        // Write Inode to Inode Table (Sector 19)
        // Index 4 (Fifth inode)
        fseek(disk_fp, sb.inode_table_block * PROJ_BLOCK_SIZE + 4 * sizeof(sfs_inode), SEEK_SET);
        fwrite(&thread_inode, 1, sizeof(thread_inode), disk_fp);

        // Write Data
        printf("Writing thread_test.elf Data...\n");
        uint8_t *thread_data = (uint8_t *)malloc(thread_size);
        fread(thread_data, 1, thread_size, thread_fp);

        fseek(disk_fp, next_free_block * PROJ_BLOCK_SIZE, SEEK_SET);
        fwrite(thread_data, 1, thread_size, disk_fp);

        free(thread_data);
        next_free_block += needed_blocks;
        fclose(thread_fp);
    }
    
    printf("Updating Inode Bitmap...\n");

    // Move to sector 18 where the bitmap is
    fseek(disk_fp, sb.inode_bitmap_block * PROJ_BLOCK_SIZE, SEEK_SET);

    uint8_t bitmap[512] = {0}; 
    bitmap[0] = 0x1F; // 0001 1111 -> 5 inodes used (kernel, hello, shell, fork_cow, thread_test)

    fwrite(bitmap, 1, 512, disk_fp);

    fclose(disk_fp);
    printf("Successfully created disk.img!\n");
    return 0;
}
