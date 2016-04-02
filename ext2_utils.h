#include "ext2.h"

unsigned char *disk;
unsigned char *INODE_BITMAP_START; /* pointer to start of inodes bitmap */
unsigned char *BLOCK_BITMAP_START; /* pointer to start of blocks bitmap */

unsigned int inodes_count;
unsigned int blocks_count;

struct ext2_super_block *super_block;
struct ext2_group_desc *block_group;

struct ext2_inode *inode; /* pointer to "inode" 0. n_th inode at inode[n] */
struct ext2_block *block; /* pointer to "block" 0. n_th block at block[n] */




/* DISK INITIALIZATION */

/*
 * Maps img file to memory and sets up global variables.
 */
void disk_initialization(char *filename);




/* BITMAP OPERATIONS */

void unset_bit(unsigned char *first, int start_index, int target_index);
void set_bit(unsigned char *first, int start_index, int target_index);
int check_bit(unsigned char *first, int start_index, int target_index);




/* BITMAP INODE OPERATIONS */

/*
 * Sets bitmap bit representing inode at index target_index.
 */
void inode_bitmap_set(int target_index);


/*
 * Unsets bitmap bit representing inode at index target_index.
 */
void inode_bitmap_unset(int target_index);


/*
 * Returns 0 if inode at index target_index is allocated, or 1 otherwise.
 */
int inode_available(int target_index);




/* BITMAP BLOCK OPERATIONS */

/*
 * Sets bitmap bit representing block at index target_index.
 */
void block_bitmap_set(int target_index);


/*
 * Unsets bitmap bit representing block at index target_index.
 */
void block_bitmap_unset(int target_index);


/*
 * Returns 0 if block at index target_index is allocated, or 1 otherwise.
 */
int block_available(int target_index);




/* ALLOCATION / DEALLOCATION */

/*
 * Searches for available block. Allocates block if found, returning the index,
 * or exits with ENOMEM otherwise.
 */
unsigned int allocate_block();


/*
 * Searches for available inode. Allocates inode if found, returning the index,
 * or exits with ENOMEM otherwise.
 */
unsigned int allocate_inode();


/*
 * Recycles block at given index.
 */
void free_block(int block_index);


/*
 * Recycles inode at given index.
 */
void free_inode(int inode_index);


/*
 * Zeroes out the block.
 */
void clear_block(struct ext2_block *b);




/* COPY OPERATIONS */

/*
 * Returns a copy of the given string.
 */
char *copy_str(char *str);


/*
 * Replicates data from src block to dest block.
 */
void copy_block(struct ext2_block *blk_src, struct ext2_block *blk_dest);


/*
 * Replicates data from inode at src index and returns inode index of copy.
 */
void copy_inode(unsigned int inode_src, unsigned int inode_dest);




/* DIRECTORY OPERATIONS */

/*
 * Returns smallest multiple of alignment greater than value.
 */
unsigned int align_to_nearest(unsigned int alignment, unsigned int value);


/*
 * Returns the total space used by directory entry.
 */
unsigned int dir_entry_size(unsigned int name_len);


/*
 * Checks the total space used by directory entry and the total space allocated
 * for directory entry against required space. Returns 0 if entry can accomodate,
 * 1 otherwise (if full).
 */
int dir_entry_full(struct ext2_dir_entry_2 *de, int required_space);


/*
 * If it exists, sets pointer to next directory entry in block, and returns 1,
 * othersise sets pointer to NULL and returns 0.
 */
int dir_entry_next(struct ext2_dir_entry_2 **cur, int *remaining);


/*
 * Returns pointer to directory entry of filename in directory given by inode index,
 * or NULL otherwise.
 */
struct ext2_dir_entry_2 *dir_find(unsigned int dir_inode, char *filename);


/*
 * Adds entry with given fields to directory with given directory inode index.
 */
void add_entry(unsigned int dir_inode, unsigned int inode, int name_len, int file_type, char *filename);


/*
 * Deletes entry for given filename in directory with given directory inode index.
 */
void delete_entry(unsigned int  dir_inode, char *filename);


/*
 * Initializes a new directory, including adding "." and ".." directory entries.
 */
void init_dir_inode(unsigned int new_dir_inode, unsigned int parent_dir_inode, char *dir_filename);


/*
 * Prints the directory given by inode index.
 */
void print_dir(unsigned int dir_inode);




/* INODE OPERATIONS */

struct ptr_with_err {
	void *ptr;
	int err; 
};

struct next_slot_state_i {
	unsigned int index[4];
	unsigned int indirection;
	unsigned int *start_slot_ptr;
};

struct next_slot_state {
	struct ext2_inode *in;
	unsigned int block_index;
	struct next_slot_state_i indirection_state;
};

enum {
	NO_ERR											= 0,
	INDIRECTION_BLOCK_MISSING		= 1,
	DATA_BLOCK_MISSING					= 2
};


/*
 * Initializes indirection state for next_slot_i function.
 */
void initialize_state_i(struct next_slot_state_i *state, unsigned int *start_slot_ptr, unsigned int indirection);


/*
 * Initializes state for next_slot function.
 */
void initialize_state(struct next_slot_state *state, unsigned int inode_index);


/*
 * Handles indirection for next_slot function.
 */
struct ptr_with_err next_slot_i(struct next_slot_state_i *state);


/*
 * Returns pointer (with error code) to the slot expected to contain next data block index 
 * for inode specified by state 
 */
struct ptr_with_err next_slot(struct next_slot_state *state);


/*
 * Returns pointer in inode with given inode index to first available slot to store 
 * a new block index.
 */
unsigned int *inode_free_slot(int inode_index);


/*
 * Adds block with given block index to inode with given inode index.
 */
void inode_add_block(int inode_index, int block_index);


/*
 * Frees and removes block with given block index from inode with given inode index.
 */
void inode_remove_block(int inode_index, int block_index);


/*
 * Reads blocks from fd and adds data to inode at inode_index
 */
void populate_inode(unsigned int inode_index, FILE *stream);


/*
 * Checks if inode at given index has specified file type.
 */
int has_file_type(int filetype, unsigned int inode_index);




/* PATH OPERATIONS */

/*
 * Returns the path to the parent directory of target file in given path.
 */
char *extract_parent_path(char *path);


/*
 * Returns the target filename of the given path. Modifies provided path.
 */

char *extract_filename_unsafe(char *path);


/*
 * Returns the target filename of the given path.
 */
char *extract_filename(char *path);


/*
 * Returns index of inode determined by path if it exists, or exits with
 * ENOENT otherwise.
 */
unsigned int inode_from_path(char *path);

/*
 * Returns index of inode determined by path stored in symbolic link
 */
unsigned int inode_from_symlink(unsigned int sym_inode_index);



/* DEBUG */

void print_result(struct ptr_with_err result);
void print_state_i(struct next_slot_state_i state);
void print_state(struct next_slot_state state);
void print_block(struct ext2_block *blk);
void print_file(unsigned int inode_index);

