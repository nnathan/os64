#ifndef _SYS_STAT_H
#define _SYS_STAT_H

struct stat
{
    dev_t   st_dev;
    ino_t   st_ino;
    mode_t  st_mode;
    nlink_t st_nlink;
    uid_t   st_uid;
    gid_t   st_gid;
    dev_t   st_rdev;
    off_t   st_size;
    time_t  st_atime;
    time_t  st_mtime;
    time_t  st_ctime;
};

#define S_IFMT      0170000
#define     S_IFDIR     0040000     /* directory */
#define     S_IFCHR     0020000     /* char special */
#define     S_IFBLK     0060000     /* block special */
#define     S_IFREG     0100000     /* regular file */ 

#define S_ISUID     0004000     /* set UID */
#define S_ISGID     0002000     /* set GID */
#define S_ISVTX     0001000     /* sticky */

#define S_IRWXU     0000700         /* owner perms */
#define     S_IRUSR     0000400     
#define     S_IWUSR     0000200
#define     S_IXUSR     0000100

#define S_IRWXG     0000070         /* group perms */
#define     S_IRGRP     0000040     
#define     S_IWGRP     0000020
#define     S_IXGRP     0000010

#define S_IRWXO     0000007         /* others' perms */
#define     S_IROTH     0000004     
#define     S_IWOTH     0000002
#define     S_IXOTH     0000001

#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)

#endif /* _SYS_STAT_H */
