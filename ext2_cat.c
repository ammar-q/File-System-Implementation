#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc != 3) {
		fprintf(stderr, "Usage: ext2_cat <image file name> <path to file>\n");
		exit(1);
	}
	disk_initialization(argv[1]); /* initialize disk */
	unsigned int inode_index = inode_from_path(argv[2]); /* get inode index from path */

	if (has_file_type(EXT2_INODE_FT_DIR, inode_index)) { 
		fprintf(stderr, "%s: is a directory\n", argv[2]);
		exit(EISDIR);
	}
	else {
		/* print the file contents */
		print_file(inode_index);
	}
	return 0;
}
