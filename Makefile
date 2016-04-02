all: clean ext2_utils.o ext2_ls ext2_rm ext2_ln ext2_mkdir ext2_cp ext2_cat ext2_mv ext2_cp2

clean : 
	rm -f *.o

ext2_utils.o : ext2_utils.c ext2_utils.h ext2.h 
	gcc -Wall -c $<

%: %.c ext2_utils.o
	gcc -Wall $^ -o $*
