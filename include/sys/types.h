#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

typedef unsigned int ino_t;
typedef long time_t;
typedef long off_t;
typedef unsigned short mode_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;
typedef unsigned short nlink_t;
typedef int pid_t;
typedef long daddr_t;

typedef unsigned dev_t;

#define makedev(major, minor)   (((major) << 16) | (minor))
#define major(dev)              (((dev) >> 16) & 0xFFFF)
#define minor(dev)              ((dev) & 0xFFFF)

typedef unsigned pgno_t;

#define ADDR_TO_PGNO(addr)  ((pgno_t) (((unsigned long) addr) >> PAGE_SHIFT))
#define PGNO_TO_ADDR(pgno)  (((unsigned long) pgno) << PAGE_SHIFT)

#endif /* _SYS_TYPES_H */
