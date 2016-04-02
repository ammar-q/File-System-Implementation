#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ext2_utils.h"

int main(int argc, char **argv) {
	if(argc != 4) {
		fprintf(stderr, "Usage: ext2_cp <image file name> <path in native fs> <path in target fs>\n");
		exit(1);
	}
	disk_initialization(argv[1]); /* initialize disk */

	FILE *data_stream;
	if ((data_stream = fopen(argv[2], "r"))) {
		/* get parent directory of destination */
		unsigned int inode_dir = inode_from_path(extract_parent_path(argv[3]));
		/* get file name of destination */
		char *filename = extract_filename(argv[3]);

		if (dir_find(inode_dir, filename)) {
			// Case: directory entry with file name already exists
			fprintf(stderr, "%s: already exists\n", argv[3]);
			exit(EEXIST);
		}

		/* allocate inode for new file */
		unsigned int new_inode_index = allocate_inode();

		/* populate the inode with data from file */
		populate_inode(new_inode_index, data_stream);
		inode[new_inode_index].i_mode |= 8 << 12;

		/* add new inode to destination directory */
		add_entry(inode_dir, new_inode_index, strlen(filename), EXT2_FT_REG_FILE, filename);
	}
	else {
		// Case: file to copy could not be opened
		fprintf(stderr, "%s : failed to open file.\n", argv[2]);
		exit(ENOENT);
	}

        return 0;
}
