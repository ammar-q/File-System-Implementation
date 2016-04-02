#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc != 3) {
		fprintf(stderr, "Usage: ext2_mkdir <image file name> <path to file>\n");
		exit(1);
	}
	disk_initialization(argv[1]); /* initialize disk */

	/* retrieve the inode index for parent directory */
	unsigned int inode_dir = inode_from_path(extract_parent_path(argv[2]));
	
	/* get filename for new directory */
	char *filename = extract_filename(argv[2]);

	if (dir_find(inode_dir, filename)) {
		// Case: filename already exists
		fprintf(stderr, "%s: already exists\n", argv[2]);
		exit(EEXIST);
	}

	/* allocate inode for new directory */
	unsigned int new_inode = allocate_inode();

	/* initialize the new directory inode and add as entry to parent directory */
	init_dir_inode(new_inode, inode_dir, filename); 

	return 0;
}
