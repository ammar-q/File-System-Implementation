#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc != 4) {
		fprintf(stderr, "Usage: ext2_mv <image file name> <path to src> <path to dest>\n");
		exit(1);
	}
	disk_initialization(argv[1]); /* initialize disk */

	/* get inode of src file */
	unsigned int inode_src = inode_from_path(extract_parent_path(argv[2]));
	unsigned int inode_dest = inode_from_path(extract_parent_path(argv[3]));


	struct ext2_dir_entry_2 *de_src = dir_find(inode_src, extract_filename(argv[2]));

	if (dir_find(inode_dest, extract_filename(argv[3]))) {
		fprintf(stderr, "%s: already exists\n", argv[3]);
		exit(EEXIST);
	}

	add_entry(inode_dest, de_src->inode, strlen(extract_filename(argv[3])), de_src->file_type, extract_filename(argv[3]));
	
	if (has_file_type(EXT2_INODE_FT_DIR, de_src->inode)){
		dir_find(de_src->inode, "..")->inode = inode_dest;
	}

	delete_entry(inode_src, extract_filename(argv[2]));
	return 0;
}