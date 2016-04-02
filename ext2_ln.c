#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc < 4) {
		fprintf(stderr, "Usage: ext2_ln <image file name> [-s] <path to file> <path to link>\n");
		exit(1);
	}
	disk_initialization(argv[1]); /* initialize disk */

	if (argv[2][0] == '-') {
		if (argc != 5 || strcmp(argv[2], "-s")) {
			fprintf(stderr, "Usage: ext2_ln <image file name> [-s] <path to file> <path to link>\n");
			exit(1);
		}

		/* get inode index of directory where link is being created */
		unsigned int inode_dir = inode_from_path(extract_parent_path(argv[4]));

		/* get name of symbolic link */
		char *filename = extract_filename(argv[4]);

		if (dir_find(inode_dir, filename)) {
			// Case: if directory already contains entry by the same name

			if (has_file_type(EXT2_INODE_FT_DIR, inode_from_path(argv[4]))) {
				// Case: if entry is a directory
				fprintf(stderr, "%s: is a directory\n", argv[4]);
				exit(EISDIR);
			}

			fprintf(stderr, "%s: already exists\n", argv[4]);
			exit(EEXIST);
		}

		FILE *path_stream = fmemopen(argv[3], strlen(argv[3]), "r");
		unsigned int new_inode_index = allocate_inode();
		populate_inode(new_inode_index, path_stream);
		inode[new_inode_index].i_mode |= 10 << 12;
		add_entry(inode_dir, new_inode_index, strlen(filename), EXT2_FT_SYMLINK, filename);
	}
	else {
		/* get directory of file in parent directory */
		struct ext2_dir_entry_2 *de = dir_find(inode_from_path(extract_parent_path(argv[2])), extract_filename(argv[2]));

		/* get inode index of directory where link is being created */
		unsigned int inode_dir = inode_from_path(extract_parent_path(argv[3]));

		/* get name of hard link */
		char *filename = extract_filename(argv[3]);

		if (dir_find(inode_dir, filename)) {
			// Case: if directory already contains entry by the same name

			if (has_file_type(EXT2_INODE_FT_DIR, inode_from_path(argv[3]))) {
				// Case: if entry is a directory
				fprintf(stderr, "%s: is a directory\n", argv[3]);
				exit(EISDIR);
			}

			fprintf(stderr, "%s: already exists\n", argv[3]);
			exit(EEXIST);
		}
		if (has_file_type(EXT2_INODE_FT_DIR, de->inode)) {
			// Case: if file being linked to is a directory
			fprintf(stderr, "%s: is a directory\n", argv[2]);
			exit(EISDIR);
		}

		/* add directory entry for hardlink */
		add_entry(inode_dir, de->inode, strlen(filename), de->file_type, filename);
	}

    return 0;
}
