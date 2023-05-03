Note: We moved disk struct in disk.c into disk.h to prevent linking issue we were seeing. No other changes were made to disk.c or disk.h.
in fs_create we initialize all directs to 0

Descriptions of functions we added:
Scan_bitmap: find the next free block in the bitmap
ld_inode: retrieve the inode information
save_inode: store inode into disk



