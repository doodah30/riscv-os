#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// 从进程的打开文件表中获取参数 fd 对应的文件指针
static int argfd(int n, int *pfd, struct file **pf) {
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0) return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd) *pfd = fd;
  if(pf) *pf = f;
  return 0;
}

// 分配一个文件描述符
static int fdalloc(struct file *f) {
  struct proc *p = myproc();
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    // now that success is guaranteed:
    dp->nlink++;  // for ".."
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  // something went wrong. de-allocate ip.
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

// SYS_read(fd, buf, n)
uint64 sys_read(void) {
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

// SYS_write(fd, buf, n)
uint64 sys_write(void) {
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return filewrite(f, p, n);
}

// SYS_close(fd)
uint64 sys_close(void) {
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0) return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

// SYS_fstat(fd, scuct stat*)
uint64 sys_fstat(void) {
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

// SYS_dup(fd)
uint64 sys_dup(void) {
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0) return -1;
  if((fd=fdalloc(f)) < 0) return -1;
  filedup(f);
  return fd;
}

// SYS_open(path, omode)
uint64 sys_open(void) {
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  // 获取路径参数
  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op(); // 开始文件系统事务

  if(omode & O_CREATE){
    // 创建文件
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    // 打开文件
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  // 分配文件结构和文件描述符
  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f) fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  iunlock(ip);
  end_op(); // 结束事务

  return fd;
}

// SYS_mknod(path, major, minor)
uint64 sys_mknod(void) {
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  if((argstr(0, path, MAXPATH)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0)
    return -1;

  begin_op();
  if((ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

// SYS_mkdir(path)
uint64 sys_mkdir(void) {
  char path[MAXPATH];
  struct inode *ip;

  if(argstr(0, path, MAXPATH) < 0) return -1;

  begin_op();
  if((ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

// SYS_chdir(path)
uint64 sys_chdir(void) {
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  if(argstr(0, path, MAXPATH) < 0) return -1;

  begin_op();
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  
  // 更新当前工作目录
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

extern int exec(char *path, char **argv);

uint64 sys_exec(void) {
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  // 获取路径
  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  
  memset(argv, 0, sizeof(argv));
  for(i=0; ; i++){
    if(i >= NELEM(argv))
      goto bad;
    
    // 获取 argv[i] 的地址
    if(fetchaddr(uargv+sizeof(uint64)*i, &uarg) < 0)
      goto bad;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    
    // 分配内核内存暂存参数
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    
    // 从用户空间拷贝参数字符串
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }
  int ret = exec(path, argv);
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64 sys_unlink(void) {
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  // 查找父目录
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // 确保不能删除 "." 和 ".."
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();
  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

// 获取系统启动时间 (Ticks)
uint64 sys_uptime(void) {
  uint xticks;
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}