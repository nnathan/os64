#ifndef _DIR_H
#define _DIR_H

#define DIRSIZ 28

struct direct
{
    ino_t   d_ino;
    char    d_name[DIRSIZ];
};

#endif /* _DIR_H */
