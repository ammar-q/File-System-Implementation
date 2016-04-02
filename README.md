# File-System-Implementation

This is an implementation of the ext2 file system for an assignment in an operating system course. The implementation handles all three levels of indirection. I have implemented symbolic links, and incorporated it with other commands. It expects the -s flag immediately after providing the image file. Note that I haven't yet prevented a self referencing cycle of symbolic links, and (I think) my program might break if that is attempted.
