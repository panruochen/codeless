## CL (CodeLess)

在阅读大型C/C++工程源码的过程中常常遇到以下问题
- 源码中存在的条件编译语句；有的甚至很长，超出了屏幕的高度
- 工程中存在大量同名的符号

这些都会打断思维过程，使得我不得不停下来花费时间精力去判断哪个代码块为“真”，哪个为“假”。
CL针对这个问题提供了一套简单的解决方案，在编译过程中模仿预处理器计算条件编译语句的值，然后删除“假”的代码块，
仅剩下有效代码。

### 运行环境
- Ubuntu （其它的linux发行版应该也可以，但是没做过测试）
- Python 2
- Gnu awk
- Bash
- Gnu make

### 运行
以linux为例，依次执行下列命令：
1. 编译CL，假设代码目录为${CL\_DIR}  
2. *make defconfig CROSS\_COMPILE=${CROSS\_COMPILE} ARCH=${ARCH}*  
3. *${CL\_DIR}/y-Make __--yz-cc=${CROSS\_COMPILE}gcc --yz-postprocess=${YOUR\_DIR} --yz-server-addr=${ANY\_VALID\_PATH}__ -j8*  
   --yz开头的参数传给CL的；--yz-cc表示编译器，--yz-postprocess表示先编译后清理源文件；一般来说只需要这两个参数就可以了。
   --yz-server-addr表示启动server并指定server的地址（该地址可以是任何有效的路径）。启动server可以减少临时文件的大小，
   并对程序的性能有轻微的改善。而其它的参数如-j8则是传给make的。

编译完成后，在linux目录下会发现大量的.bak文件，它们都是被清理过的同名的.c/.cpp/.h文件的备份。通过比较.bak与原文件，可以发现源文件中的条件编译代码已经被删除：  
![diff and see](https://cloud.githubusercontent.com/assets/1546040/4838205/0377bfea-5fe5-11e4-83d9-f0c20679ba7c.png)  
为了方便恢复代码，建议使用git或者其它的版本管理工具。  
为了保证目标文件vmlinux与源代码的一致性，还需要再次编译linux。  

### 限制
1. 只能使用下列编译器（其它的编译器未经测试）
  - gcc
  - clang
2. 并且${CROSS\_COMILE}gcc展开后只能包含字母，数字，下划线\_和减号-

### 副产品
${YOUR\_DIR}目录下的projlist.txt列出了本次编译涉及到的所有.c/.cpp/.h文件，我们可以利用它来生成一个更精确的cscope或者ctags的数据库。  
*cscope -bkq -i <(tail -n +2 projlist.txt)*  
通过这个数据库查找可以避免大量重名的符号。  
![Easy cscope](https://cloud.githubusercontent.com/assets/1546040/11395831/68b82500-93a8-11e5-94d3-27b56225ac58.png)

### y-Make的命令行参数
```
 --yz-cc=CC                 指定编译器
 --yz-postprocess=DIR       开启后处理模式，并使用DIR为其工作目录
 --yz-save-dep=FILE         将所有依赖关系保存在文件FILE中
 --yz-save-cl=FILE          将所有命令行保存在文件FILE中
 --yz-save-condvals=FILE    将所有条件编译语句的计算结果保存在文件FILE中
 --yz-save-proj=FILE        将编译中所涉及到的文件保存在文件FILE中
 --yz-server-addr=FILE      启动server，并以FILE作为server的地址
 --yz-runtime-dir=DIR       指定server的工作目录为DIR；默认为/var/tmp
 --yz-xcc=FILE              指定FILE为parser；默认为${CL\_DIR}/cl.exe
 --yz-server-program=FILE   指定FILE为server；默认为${CL\_DIR}/cl-server.exe
```

### Post Process（后处理）
后处理模式是为了解决编译过程中的实际问题而引入的，即一个.c/.cpp文件有可能被多次编译。如果在编译之前清理文件（这是默认模式），
被删除掉的代码很可能在第二次编译时应该被引用，从而导致整个编译过程失败。
并且，.h文件几乎100%会被多次引用，如果未开启后处理模式，清理.h文件是不可能的。

### 适用范围
下列开源项目经过测试
 * linux-3.6
 * linux-2.6.27.29
 * llvm-3.5.0
 * u-boot
 * make-4.0
