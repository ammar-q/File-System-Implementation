#include <stdio.h>
#include <stdlib.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc != 3) {
		fprintf(stderr, "Usage: ext2_ls <image file name> <path to directory>\n");
		exit(1);
	}

	disk_initialization(argv[1]); /* initialize disk */
	unsigned int inode_index = inode_from_path(argv[2]); /* get inode index from path */

	if (has_file_type(EXT2_INODE_FT_DIR, inode_index)) {
		// Case: inode has directory filetype
		print_dir(inode_index); /* prints the directory contents */
	}
	else {
		fprintf(stderr, "%s: is not a directory\n", argv[2]);
	}
	return 0;
}
