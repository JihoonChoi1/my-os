#include "fs.h"
#include "../drivers/ata.h"
#include "../mm/kheap.h"

// Debug functions
extern void print_string(char* str);
extern void print_hex(uint32_t n);
extern void print_dec(uint32_t n);

// Global Superblock
sfs_superblock sb;

// String Compare Helper
static int strcmp(char *s1, char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void fs_init() {
    print_string("Initializing SimpleFS...\n");
    
    // 1. Read Superblock (Sector 17)
    // Why 17? MBR(1) + Reserved(16)
    uint8_t buffer[512];
    ata_read_sector(17, buffer);
    
    sfs_superblock *read_sb = (sfs_superblock*)buffer;
    
    if (read_sb->magic != SIMPLEFS_MAGIC) {
        print_string("[FS] Error: Invalid Magic Number! Found: ");
        print_hex(read_sb->magic);
        print_string("\n");
        return;
    }
    
    // Copy to global
    sb = *read_sb;
    print_string("[FS] Mount Success! Total Blocks: ");
    print_dec(sb.total_blocks);
    print_string("\n");
}

// Find a file by name and return its Inode
// Returns 1 on success, 0 on failure
int fs_find_file(char *filename, sfs_inode *out_inode) {
    uint8_t buffer[512];
    
    // Loop through all inodes
    // Inodes are stored starting at sb.inode_table_block
    // Each block contains 512 / sizeof(sfs_inode) inodes.
    // Simplifying: Just read sector by sector and check all inodes in it.
    
    uint32_t inodes_per_block = 512 / sizeof(sfs_inode);
    uint32_t total_inode_blocks = (sb.num_inodes + inodes_per_block - 1) / inodes_per_block;
    
    for (uint32_t i = 0; i < total_inode_blocks; i++) {
        ata_read_sector(sb.inode_table_block + i, buffer);
        
        sfs_inode *inodes = (sfs_inode*)buffer;
        for (uint32_t j = 0; j < inodes_per_block; j++) {
            if (inodes[j].used == 1) {
                if (strcmp(inodes[j].filename, filename) == 0) {
                    *out_inode = inodes[j];
                    return 1; // Found!
                }
            }
        }
    }
    
    return 0; // Not Found
}

// List all files in the root directory
void fs_list_files() {
    print_string("--- File List ---\n");
    uint8_t buffer[512];
    
    uint32_t inodes_per_block = 512 / sizeof(sfs_inode);
    uint32_t total_inode_blocks = (sb.num_inodes + inodes_per_block - 1) / inodes_per_block;
    
    for (uint32_t i = 0; i < total_inode_blocks; i++) {
        ata_read_sector(sb.inode_table_block + i, buffer);
        
        sfs_inode *inodes = (sfs_inode*)buffer;
        for (uint32_t j = 0; j < inodes_per_block; j++) {
            if (inodes[j].used == 1) {
                print_string("- ");
                print_string(inodes[j].filename);
                print_string(" (");
                print_dec(inodes[j].size);
                print_string(" bytes)\n");
            }
        }
    }
    print_string("-----------------\n");
}

// Read file content into buffer
void fs_read_file(sfs_inode *inode, char *buffer) {
    uint32_t needed_blocks = (inode->size + 511) / 512;
    
    for (uint32_t i = 0; i < needed_blocks; i++) {
        // Read each block
        // Note: buffer pointer arithmetic needs care.
        ata_read_sector(inode->blocks[i], (uint8_t*)(buffer + i * 512));
    }
}
