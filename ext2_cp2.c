#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc != 4) {
		fprintf(stderr, "Usage: ext2_cp2 <image file name> <path to src> <path to dest>\n");
		exit(1);
	}
	disk_initialization(argv[1]); /* initialize disk */

	/* get inode of src file */
	unsigned int inode_src = inode_from_path(argv[2]);

	/* get inode index of directory where copy is being created */
	unsigned int inode_dir = inode_from_path(extract_parent_path(argv[3]));

	/* get name of new file */
	char *filename = extract_filename(argv[3]);

	if (dir_find(inode_dir, filename)) {
		// Case: if destination dir already contains destination filename in dir entry
		fprintf(stderr, "%s: already exists\n", argv[3]);
		exit(EEXIST);
	}
	if (has_file_type(EXT2_INODE_FT_DIR, inode_src)) {
		// Case: if file being copied is a directory
		fprintf(stderr, "%s: is a directory\n", argv[2]);
		exit(EISDIR);
	}

	/* allocate inode for copy */
	unsigned int inode_dest = allocate_inode();
	/* add directory entry for copy */
	add_entry(inode_dir, inode_dest, strlen(filename), EXT2_FT_REG_FILE, filename);
	/* copy data */
	copy_inode(inode_src, inode_dest);
	return 0;
}