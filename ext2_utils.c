#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "ext2_utils.h"


/* DISK INITIALIZATION */

void disk_initialization(char *filename){
	int fd = open(filename, O_RDWR);
	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if(disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	super_block = (struct ext2_super_block *)(disk + 1024);
	block_group = (struct ext2_group_desc *) (disk + 2*1024);

	BLOCK_BITMAP_START = disk + block_group->bg_block_bitmap * EXT2_BLOCK_SIZE;
	INODE_BITMAP_START = disk + block_group->bg_inode_bitmap * EXT2_BLOCK_SIZE;

	inode = (struct ext2_inode *) ((unsigned char *)disk + block_group->bg_inode_table * EXT2_BLOCK_SIZE);
	inode --;
	block = (struct ext2_block *) disk;

	inodes_count = super_block->s_inodes_count;
	blocks_count = super_block->s_blocks_count;
}





/* BITMAP OPERATIONS */

void unset_bit(unsigned char *first, int start_index, int target_index) {
	unsigned char *byte_to_modify = first + ((target_index - start_index) >> 3);
	int bit_to_modify = (target_index - start_index) % 8;
	*byte_to_modify = (*byte_to_modify) & (~(1 << bit_to_modify));
}


void set_bit(unsigned char *first, int start_index, int target_index) {
	unsigned char *byte_to_modify = first + ((target_index - start_index) >> 3);
	int bit_to_modify = (target_index - start_index) % 8;
	*byte_to_modify = (*byte_to_modify) | (1 << bit_to_modify);
}


int check_bit(unsigned char *first, int start_index, int target_index) {
	unsigned char *byte_to_check = first + ((target_index - start_index) >> 3);
	int bit_to_check = (target_index - start_index) % 8;
	return ((*byte_to_check)>>bit_to_check) & 1;
}




/* BITMAP INODE OPERATIONS */

void inode_bitmap_set(int target_index) {
	set_bit(INODE_BITMAP_START, 1, target_index);
}


void inode_bitmap_unset(int target_index) {
	unset_bit(INODE_BITMAP_START, 1, target_index);
}


int inode_available(int target_index) {
	return !check_bit(INODE_BITMAP_START, 1, target_index);
}




/* BITMAP BLOCK OPERATIONS */

void block_bitmap_set(int target_index) {
	set_bit(BLOCK_BITMAP_START, 1, target_index);
}


void block_bitmap_unset(int target_index) {
	unset_bit(BLOCK_BITMAP_START, 1, target_index);
}


int block_available(int target_index) {
	return !check_bit(BLOCK_BITMAP_START, 1, target_index);
}




/* ALLOCATION / DEALLOCATION */

unsigned int allocate_block() {
	unsigned int i;
	for (i = 1; i <= blocks_count; i++) {
		if (block_available(i)) {
			// Case: block[i] available
			block_bitmap_set(i);
			super_block->s_free_blocks_count --;
			block_group->bg_free_blocks_count --;
			clear_block(&block[i]);
			return i;
		}
	}  
	// Case: no blocks available
	fprintf(stderr, "failed to allocate block");
	exit(ENOMEM);
}


unsigned int allocate_inode() {
	unsigned int i;
	for (i = 12; i <= inodes_count; i++) {
		if (inode_available(i)) {
			// Case: inode[i] available
			inode_bitmap_set(i);
			super_block->s_free_inodes_count --;
			block_group->bg_free_inodes_count --;
			inode[i].i_size = 0;
			inode[i].i_mode = (inode[i].i_mode << 4) >> 4;
			inode[i].i_links_count = 0;
			inode[i].i_blocks = 0;
			return i;
		}
	}
	// Case: no inodes available
	fprintf(stderr, "failed to allocate inode");
	exit(ENOMEM);
}


void free_block(int block_index) {
	block_bitmap_unset(block_index);
	super_block->s_free_blocks_count ++;
	block_group->bg_free_blocks_count ++;
}


void free_inode(int inode_index) {
  struct ptr_with_err result;
	struct next_slot_state state;
	initialize_state(&state, inode_index);

	/* free all associated blocks */
	while (1) {
		result = next_slot(&state);
		if (result.ptr != NULL && result.err == NO_ERR) {
			free_block(*((unsigned int *) result.ptr));
			*((unsigned int *) result.ptr) = 0;
		}
		else {
			break;
		}
	}

	/* free inode */
	inode_bitmap_unset(inode_index);
	super_block->s_free_inodes_count ++;
	block_group->bg_free_inodes_count ++;
	inode[inode_index].i_links_count = 0;
}


void clear_block(struct ext2_block *b) {
	unsigned int i;
	for (i = 0; i < EXT2_ADDR_PER_BLOCK; i++) {
		b->addr[i] = 0; 
	}
}



/* COPY OPERATIONS */

char *copy_str(char *str) {
	char *copy = malloc(strlen(str) + 1);
	strcpy(copy, str);
	return copy;
}


void copy_block(struct ext2_block *blk_src, struct ext2_block *blk_dest) {
	unsigned int i;
	for (i = 0; i < EXT2_ADDR_PER_BLOCK; i++) {
		blk_dest->addr[i] = blk_src->addr[i]; 
	}
}


void copy_inode(unsigned int inode_src, unsigned int inode_dest) {
	struct ptr_with_err result;
	struct next_slot_state state;

	initialize_state(&state, inode_src);
	while (1) {
		result = next_slot(&state);
		if (result.ptr != NULL && result.err == NO_ERR) {
			unsigned int new_block = allocate_block();
			inode_add_block(inode_dest, new_block);
			copy_block(&block[*((unsigned int *) result.ptr)], &block[new_block]);
		}
		else {
			break;
		}
	}

	inode[inode_dest].i_blocks = inode[inode_src].i_blocks;
	inode[inode_dest].i_mode = inode[inode_src].i_mode;
	inode[inode_dest].i_size = inode[inode_src].i_size;
	inode[inode_dest].i_links_count = inode[inode_src].i_links_count;
}


/* DIRECTORY OPERATIONS */

unsigned int align_to_nearest(unsigned int alignment, unsigned int value){
	return (value % alignment) ? (value + alignment - (value % alignment)): value;
}

unsigned int dir_entry_size(unsigned int name_len){
	return align_to_nearest(4, sizeof(struct ext2_dir_entry_2) + name_len);
}


int dir_entry_full(struct ext2_dir_entry_2 *de, int required_space){
	return (de->rec_len - dir_entry_size(de->name_len) < required_space)? 1 : 0;
}


int dir_entry_next(struct ext2_dir_entry_2 **cur, int *remaining) {
	if (*remaining - (*cur)->rec_len > 0) {
		*remaining = *remaining - (*cur)->rec_len;
		*cur = (struct ext2_dir_entry_2 *) (((unsigned char *)(*cur)) + (*cur)->rec_len);
		return 1; 
	} 
	else 
		return 0;
}


struct ext2_dir_entry_2 *dir_find(unsigned int dir_inode, char *filename) {
	struct next_slot_state state;
	struct ptr_with_err result;
	initialize_state(&state, dir_inode);

	/* cycle through all blocks */
	while (1) {
		result = next_slot(&state);

		if (result.ptr != NULL && result.err == NO_ERR) {
			struct ext2_dir_entry_2 *cur = (struct ext2_dir_entry_2 *) &block[*((unsigned int *) result.ptr)];
			int remaining = EXT2_BLOCK_SIZE;

			/* cycle through all directory entries in block */
			do {
				if ((strlen(filename) == cur->name_len) && !strncmp(filename, cur->name, cur->name_len)) {
					return cur;
				}
			}
			while (dir_entry_next(&cur, &remaining));
		} 
		else break;
	}

	return NULL;
}


void add_entry(unsigned int dir_inode, unsigned int new_inode, int name_len, int file_type, char *filename){ 
	inode[new_inode].i_links_count ++;

	struct ext2_dir_entry_2 *new;
	struct next_slot_state state;
	struct ptr_with_err result;
	initialize_state(&state, dir_inode);

	int required_space = dir_entry_size(strlen(filename));

	/* cycle through all blocks */
	while (1) {
		result = next_slot(&state);
		if (result.ptr != NULL && result.err == NO_ERR) {
			struct ext2_dir_entry_2 *cur = (struct ext2_dir_entry_2 *) &block[*((unsigned int *) result.ptr)];
			int remaining = EXT2_BLOCK_SIZE;

			/* cycle through all directory entries in block */
			do {
				if (!dir_entry_full(cur, required_space)) {
					new = (struct ext2_dir_entry_2 *)(((unsigned char *) cur) + dir_entry_size(cur->name_len));

					new->rec_len = cur->rec_len - dir_entry_size(cur->name_len);
					cur->rec_len = dir_entry_size(cur->name_len);

					new->inode = new_inode;
					new->name_len = name_len;
					new->file_type = file_type;
					strncpy(new->name, filename, strlen(filename));
					return;
				}
			}
			while (dir_entry_next(&cur, &remaining));
		} 
		else break;
	}

	// Case: no associated block can accomodate new entry
	int new_block = allocate_block();
	inode_add_block(dir_inode, new_block);
	inode[dir_inode].i_size += EXT2_BLOCK_SIZE;
	new =  (struct ext2_dir_entry_2 *) (&block[new_block]);
	new->rec_len = EXT2_BLOCK_SIZE;
	new->inode = new_inode;
	new->name_len = name_len;
	new->file_type = file_type;
	strncpy(new->name, filename, strlen(filename));
}


void delete_entry(unsigned int dir_inode, char *filename){
	struct ext2_dir_entry_2 *dbe = dir_find(dir_inode, filename);
	if (dbe) {
		unsigned int dbe_inode = dbe->inode;
		unsigned int dir_block = (((unsigned char *) dbe) - disk) / EXT2_BLOCK_SIZE;
		struct ext2_dir_entry_2 *cur = (struct ext2_dir_entry_2 *) &block[dir_block];
		struct ext2_dir_entry_2 *last_dbe;
		int remaining = EXT2_BLOCK_SIZE;
		int rest;
		do {
			if (cur == dbe) {
				rest = remaining;
			}
			else {
				last_dbe = cur;
			}
		}
		while (dir_entry_next(&cur, &remaining));

		if (last_dbe){
			last_dbe->rec_len += dbe->rec_len;
			/* move the rest of the directory entries back */
			memmove((void *)dbe, (void *) (((unsigned char *) dbe) + dbe->rec_len), rest - dbe->rec_len);
		} 
		else {
			inode_remove_block(dir_inode, dir_block);
			inode[dir_inode].i_size -= EXT2_BLOCK_SIZE;
			free_block(dir_block);
		}

		inode[dbe_inode].i_links_count --;
		if (inode[dbe_inode].i_links_count == 0) {
			free_inode(dbe_inode);
		}
	}
}


void init_dir_inode(unsigned int new_dir_inode, unsigned int parent_dir_inode, char *dir_filename){
	/* add . and .. directory entries for new directory */
	add_entry(new_dir_inode, new_dir_inode, strlen("."), EXT2_FT_DIR, ".");
	add_entry(new_dir_inode, parent_dir_inode, strlen(".."), EXT2_FT_DIR, "..");

	/* update metadata */
	inode[new_dir_inode].i_mode = inode[parent_dir_inode].i_mode;
	block_group->bg_used_dirs_count ++;

	/* add directory entry for new directory in parent directory */
	add_entry(parent_dir_inode, new_dir_inode, strlen(dir_filename), EXT2_FT_DIR, dir_filename);
}


void print_dir(unsigned int dir_inode) {
	struct next_slot_state state;
	struct ptr_with_err result;
	initialize_state(&state, dir_inode);

	/* cycle through all blocks */
	while (1) {
		result = next_slot(&state);
		if (result.ptr != NULL && result.err == NO_ERR) {
			struct ext2_dir_entry_2 *cur = (struct ext2_dir_entry_2 *) &block[*((unsigned int *) result.ptr)];
			int remaining = EXT2_BLOCK_SIZE;

			/* cycle through all directory entries in block */
			do {
				printf("%.*s\n", cur->name_len, cur->name);
			}
			while (dir_entry_next(&cur, &remaining));

		} 
		else break;
	}
}




/* INODE OPERATIONS */

void initialize_state_i(struct next_slot_state_i *state, unsigned int *start_slot_ptr, unsigned int indirection) {
	int i;
	for (i = 0; i < 4; i++) 
		state->index[i] = 0;
	state->start_slot_ptr = start_slot_ptr;
	state->indirection = indirection;
}


void initialize_state(struct next_slot_state *state, unsigned int inode_index) {
	state->in = &inode[inode_index];
	state->block_index = 0;
	initialize_state_i(&(state->indirection_state), state->in->i_block + state->block_index, 0);
}


struct ptr_with_err next_slot_i(struct next_slot_state_i *state) {
	unsigned int *index = state->index;
	unsigned int indirection = state->indirection;
	unsigned int *start_slot_ptr = state->start_slot_ptr;
	struct ptr_with_err result;
	int i;

	if (!index[0]) {
		unsigned int *slot = start_slot_ptr;
		for (i = 1; i <= indirection; i++) {

			if (*slot == 0) {
				// Case: Error. Indirection block missing.
				result.ptr = slot;
				result.err = INDIRECTION_BLOCK_MISSING;
				return result;
			}

			/* move down one level of indirection */
			slot = block[*slot].addr + index[i];
		}

		if (*slot == 0) {
			// Case: Error. Data block missing.
			result.ptr = slot;
			result.err = DATA_BLOCK_MISSING;
			return result;
		}


		// Case: No error. Data block found. Advance for next time.
		result.ptr = slot;
		result.err = NO_ERR;

		// Advance indices by one so that next call to next_block_i
		// returns the next slot.
		int add = 1;
		for (i = indirection; i >= 0; i--) {
			if (index[i] + add < EXT2_ADDR_PER_BLOCK) {
				index[i] = index[i] + add;
				add = 0;
			} else {
				index[i] = 0;
			}
		}

		return result;
	}

	// Case: No error. Data blocks at indirection level exhausted.
	result.ptr = NULL;
	result.err = NO_ERR;
	return result;
}


struct ptr_with_err next_slot(struct next_slot_state *state) {
	struct ptr_with_err result;

	int indirection = state->block_index >= 11 ? (state->block_index - 11) : 0;

	/* cycle through the i_block pointers */
	while (state->block_index < 15) {
		struct ptr_with_err result = next_slot_i(&(state->indirection_state));

		if (result.err != NO_ERR) return result;

		if (result.ptr == NULL) {
			state->block_index ++;
			indirection = state->block_index > 11 ? (state->block_index - 11) : 0;
			initialize_state_i(&(state->indirection_state), state->in->i_block + state->block_index, indirection);
		} 
		else {
			return result;
		}
	}

	return result;
}


unsigned int *inode_free_slot(int inode_index) {
	struct ptr_with_err result;
	struct next_slot_state state;
	initialize_state(&state, inode_index); 

	while (1) {
		result = next_slot(&state);

		if (result.ptr == NULL) break;

		if (result.err == DATA_BLOCK_MISSING) {
			return (unsigned int *) result.ptr;
		}

		if (result.err == INDIRECTION_BLOCK_MISSING) {
			*((unsigned int *)(result.ptr)) = allocate_block();
		}
	}

	fprintf(stderr, "inode at full capacity");
	exit(ENOMEM);
}


void inode_add_block(int inode_index, int block_index) {
	*inode_free_slot(inode_index) = block_index;
}


void inode_remove_block(int inode_index, int block_index) {
	struct ptr_with_err result;

	unsigned int *last_data_slot = NULL;
	unsigned int *target_data_slot = NULL;

	struct next_slot_state state;
	initialize_state(&state, inode_index); 

	while (1) {
		result = next_slot(&state);
		if (result.ptr == NULL || result.err) break;
		if ( *((unsigned int *)result.ptr) == block_index) {
			target_data_slot = (unsigned int *) result.ptr;
		} else {
			last_data_slot = (unsigned int *) result.ptr;
		}
	}

	if (target_data_slot) {
		if (last_data_slot) {
			*target_data_slot = *last_data_slot;
			*last_data_slot = 0;
		} 
		else {
			*target_data_slot = 0;
		}
	}
}

void populate_inode(unsigned int inode_index, FILE *stream) {
	struct ext2_block read_block;
	clear_block(&read_block);

	int bytes, total_bytes = 0;
	while ((bytes = fread(&read_block, 1, EXT2_BLOCK_SIZE, stream)) > 0){
		total_bytes += bytes;
		/* allocate a block for new data */
		unsigned int block_index = allocate_block();
		/* copy data to new block */
		copy_block(&read_block, &block[block_index]);
		/* add block to inode */
		inode_add_block(inode_index, block_index);

		clear_block(&read_block);
	}

	/* update fields for new inode */
	inode[inode_index].i_size = total_bytes;
}

int has_file_type(int filetype, unsigned int inode_index){
	return ((inode[inode_index].i_mode >> 12) == filetype)? 1 : 0;
}


/* PATH OPERATIONS */

char *extract_parent_path(char *path) {
	char *copy;
	copy = copy_str(path);
	int index = extract_filename_unsafe(copy) - copy;
	free(copy);
	copy = copy_str(path);
	copy[index] = '\0';
	return copy;
}


char *extract_filename_unsafe(char *path) {
	char *token, *token_new;
	token = strtok(path, "/");
	while ((token_new = strtok(NULL, "/"))) {
		token = token_new;
	}
	return token;
}


char *extract_filename(char *path) {
	path = copy_str(path);
	return extract_filename_unsafe(path);
}


unsigned int inode_from_path(char *path){
	char *token;
	char *saveptr = NULL;

	path = copy_str(path);

	token = strtok_r(path, "/", &saveptr);
	unsigned int inode_index = EXT2_ROOT_INO;

	do {
		// Check if directory
		if (token && strcmp(token, "") && inode_index != 0 && has_file_type(EXT2_INODE_FT_DIR, inode_index)) {
			struct ext2_dir_entry_2 *de = dir_find(inode_index, token);
			if (de == NULL) {
				inode_index = 0;
				break;
			} 
			inode_index =  de->inode;  
			if (has_file_type(EXT2_INODE_FT_SYMLINK, inode_index)) {
				inode_index = inode_from_symlink(inode_index);
			}
		}
		else {
			if (inode_index == 0 || !has_file_type(EXT2_INODE_FT_DIR, inode_index)) inode_index = 0;
			break;
		}
	}
	while ((token = strtok_r(NULL, "/", &saveptr)));

	if (inode_index == 0) {
		fprintf(stderr, "No such file or directory\n");
		exit(ENOENT);
	}

	free(path);
	return inode_index;
}

unsigned int inode_from_symlink(unsigned int sym_inode_index){
	char *sym_path = malloc(inode[sym_inode_index].i_size);
	char *read_ptr = sym_path;

	struct next_slot_state state;
	struct ptr_with_err result;
	initialize_state(&state, sym_inode_index);

	int remaining = inode[sym_inode_index].i_size;

	/* cycle through all blocks */
	while (1) {
		result = next_slot(&state);
		if (result.ptr != NULL && result.err == NO_ERR) {
			int read_count = remaining < EXT2_BLOCK_SIZE ? remaining : EXT2_BLOCK_SIZE;
			sprintf(read_ptr, "%.*s", read_count, (char *) &block[* (unsigned int *) result.ptr]);
			read_ptr += read_count;
			remaining -= read_count;
		} 
		else break;
	}

	unsigned int inode_index = inode_from_path(sym_path);
	free(sym_path);
	return inode_index;
}


/* DEBUG */

void print_result(struct ptr_with_err result) {
	printf("ERROR %d, RESULT %d\n", result.err, *(unsigned int *)(result.ptr));
}


void print_state_i(struct next_slot_state_i state) {
	printf("Indirection Level: %d\n", state.indirection);
	printf("IDXS = [%d, %d, %d, %d]\n", state.index[0], state.index[1], state.index[2], state.index[3]);
}


void print_state(struct next_slot_state state) {
	printf("BLK_IDX: %d\n", state.block_index);
	print_state_i(state.indirection_state);
}


void print_block(struct ext2_block *blk){
	char *b =  (char *) blk;
	int i;
	for (i = 0; i < EXT2_BLOCK_SIZE; i++) {
		printf("%c", b[i]);
	}
}


void print_file(unsigned int inode_index){
	struct ptr_with_err result;
	struct next_slot_state state;
	initialize_state(&state, inode_index); 
	while (1) {
		result = next_slot(&state);
		if (result.ptr != NULL && result.err == NO_ERR) {
			print_block(&block[* (unsigned int *)result.ptr]);
		}
		else {
			printf("\n");
			break;
		}
	}
}
