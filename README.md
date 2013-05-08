PXCC
====

PXCC - A C/C++ source code CleanTool

Generally PCXX works in the following two ways:
1. The user feeds explict definitions and source files to PXCC and PCXX
   cleans those conditional blocks which can be evaluated true or false.
   If a block cannot be evaluated explictly, it will be kept.
2. PXCC works as a preprocessor attached on the `make' process and strip
   any conditional blocks in the source files which are compiled. In this
   way, PXCC must work with another run-make script with proper parameters
   set.

To understand how PXCC works, you can simply run `make run' and then get
into the directory make-3.81 to compare any c file to the bak file with
the same base name and see how different they are.
