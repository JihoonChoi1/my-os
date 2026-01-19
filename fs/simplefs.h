#ifndef SIMPLEFS_FUNC_H
#define SIMPLEFS_FUNC_H

#include "fs.h"

void fs_init();
void fs_list_files();
int fs_find_file(char *filename, sfs_inode *out_inode);
void fs_read_file(sfs_inode *inode, char *buffer);

#endif
