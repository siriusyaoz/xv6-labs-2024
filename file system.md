### 相关文件
pipe.c  //实现pipe

sysfile.c fcntl.h stat.h //open,write,close; link,unlink ;mkdir等系统调用  file descriptor

file.c  file.h //struct file

log.c  //日志

fs.c    fs.h  //inode, block, path, directory

buf.h   bio.c  //最底层直接与disk接触的buf cache

### 实验bigfile 
很简单，double indirect只要将indirect的代码再重复一遍就OK了

### 实验symlink
symlink文件可以理解成windows里的快捷方式?

它的内容存在inode的data里。先调用create 创建一个inode,在使用writei将target文件名写入到inode的data里去。注意这里的create帮我们完成了很多事，比如ialloc创建inode,调用dirlink将父文件夹和当前文件link起来。虽然我们这里的文件类型是T_SYMLINK,但是在create函数里和T_FILE走的是同一条路线。

对sys_open的改动：在没有定义O_NOFOLLOW的情况下使用辅助函数open_symlink来打卡symlink所指向的最终的真实文件。注意这里的lock操作。进入while循环时，因为要使用`dp->type`，所以是带锁的，然后解锁，调用namei得到新的`struct inode* dp`。再对dp加锁，进入下一个循环。



### 实验结果
```
== Test running bigfile == 
$ make qemu-gdb
running bigfile: OK (142.6s) 
== Test running symlinktest == 
$ make qemu-gdb
(1.7s) 
== Test   symlinktest: symlinks == 
  symlinktest: symlinks: OK 
== Test   symlinktest: concurrent symlinks == 
  symlinktest: concurrent symlinks: OK 
== Test usertests == 
$ make qemu-gdb
usertests: OK (215.5s) 
== Test time == 
time: FAIL 
    Cannot read time.txt
Score: 99/100
```
加上time.txt就满分了，我就不重跑一遍了：)


