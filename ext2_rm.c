#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc != 3) {
		fprintf(stderr, "Usage: ext2_rm <image file name> <path to file>\n");
		exit(1);
	}
	disk_initialization(argv[1]); /* initialize disk */
	unsigned int dir_inode = inode_from_path(extract_parent_path(argv[2])); /* get inode index from path */

	struct ext2_dir_entry_2 *de = dir_find(dir_inode, extract_filename(argv[2]));
	if (de == NULL) {
		fprintf(stderr, "No such file or directory\n");
		exit(ENOENT);
	}
	unsigned int inode_index = de->inode;

	if (has_file_type(EXT2_INODE_FT_DIR, inode_index)) { 
		fprintf(stderr, "%s: is a directory\n", argv[2]);
		exit(EISDIR);
	}
	else {
		/* remove directory entry associated with file */
		delete_entry(dir_inode, extract_filename(argv[2]));
	}

        return 0;
}
