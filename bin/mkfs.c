/* Copyright (c) 2018 Charles E. Youse (charles@gnuless.org). 
   All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <ctype.h>

#include "../include/sys/types.h"
#include "../include/sys/stat.h"
#include "../include/dir.h"
#include "../include/fs.h"

struct fs_super   super;
struct fs_inode   root;
FILE            * device_file;
FILE            * proto_file;
int               nr_blocks;
int               nr_inodes;
char              block[FS_BLOCK_SIZE];
char              buffer[FS_BLOCK_SIZE];
time_t            fs_time;
int               line;
int               ch;

#define ROUND_UP(a,b)       (((a) % (b)) ? ((a) + ((b) - ((a) % (b)))) : (a))

/* should be in the standard library */

bset(buf, len, i)
    char * buf;
{
    while (len--) *buf++ = i;
}

/* read or write 'lbn' */

#define BIO_READ    0
#define BIO_WRITE   1

bio(lbn, flag)
{
    int ret;

    if (fseek(device_file, (long) lbn * FS_BLOCK_SIZE, SEEK_SET)) 
        error("can't seek device");

    if (flag == BIO_WRITE)
        ret = fwrite(block, FS_BLOCK_SIZE, 1, device_file);
    else /* BIO_READ */
        ret = fread(block, FS_BLOCK_SIZE, 1, device_file);

    if (ret != 1) error("device read/write failure");
}

/* report an error and abort */

error(msg)
    char * msg;
{
    fprintf(stderr, "mkfs: ");
    if (line) fprintf(stderr, "line %d: ", line);
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/* allocation of inodes and blocks is easy (sequential),
   but track how many and make sure we're not going over. */

ialloc(inode)
    struct fs_inode * inode;
{
    int i;

    if (nr_inodes == super.nr_inodes) error("out of inodes");
    --super.nr_free_inodes;

    memset(inode, 0, sizeof(*inode));
    return nr_inodes++;
}

balloc()
{
    if (nr_blocks == super.nr_blocks) error("out of blocks");
    --super.nr_free_blocks;
    return nr_blocks++;
}

/* return the block number that holds byte offset 'ofs'
   into 'inode', allocating blocks and updating the inode as
   required. we don't go larger than single indirection. */

bmap(inode, ofs)
    struct fs_inode * inode;
{
    long * blocks;
    int    ind;

    if (ofs < FS_DIRECT_BYTES) {
        ofs /= FS_BLOCK_SIZE;
        if (inode->blocks[ofs] == 0)
            inode->blocks[ofs] = balloc();

        return inode->blocks[ofs];
    }

    ofs -= FS_DIRECT_BYTES;
    if (ofs >= FS_INDIRECT_BYTES) error("file too big");
    ind = inode->blocks[FS_INODE_INDIRECT];

    if (ind == 0) {
        ind = balloc();
        bset(block, FS_BLOCK_SIZE, 0);
        inode->blocks[FS_INODE_INDIRECT] = ind;
    } else 
        bio(ind, BIO_READ);

    ofs /= FS_BLOCK_SIZE;
    blocks = (long *) block;
    if (blocks[ofs] == 0) {
        blocks[ofs] = balloc();
        bio(ind, BIO_WRITE);
    }

    return blocks[ofs];
}

/* append 'len' bytes from 'buf' to the file 'inode'. */

iwrite(inode, buf, len)
    struct fs_inode * inode;
    char            * buf;
{
    int lbn;

    lbn = bmap(inode, inode->size);
    bio(lbn, BIO_READ);

    while (len--) {
        block[inode->size % FS_BLOCK_SIZE] = *buf++;
        inode->size++;
        if (((inode->size % FS_BLOCK_SIZE) == 0) && len) {
            bio(lbn, BIO_WRITE);
            lbn = bmap(inode, inode->size);
            bio(lbn, BIO_READ);
        }
    }

    bio(lbn, BIO_WRITE);
}

/* write a directory entry into 'inode' with 
   the given 'name' which refers to 'ino'. */

entry(inode, name, ino)
    struct fs_inode * inode;
    char            * name;
{
    static struct direct direct;

    memset(direct.d_name, 0, DIRSIZ);
    strncpy(direct.d_name, name, DIRSIZ);
    direct.d_ino = ino;
    
    iwrite(inode, &direct, sizeof(direct));
}

/* initialize the bitmap starting at LBN 'start' 
   that is 'blocks' blocks long, of which the first
   'used' elements are unavailable. */

bitmap(start, blocks, used)
{
    int i, j;

    while (blocks--) {
        for (i = 0; i < FS_BLOCK_SIZE; i++) block[i] = 0xFF;

        i = 0;
        j = 0;
        while (used && (j < FS_BLOCK_SIZE)) {
            block[j] <<= 1;
            i++;
            if (!(i % 8)) j++;
            used--;
        }

        bio(start, BIO_WRITE);
        start++;
    }
}

/* write out an inode */

iput(ino, inode)
    struct fs_inode * inode;
{
    inode->atime = fs_time;
    inode->mtime = fs_time;
    inode->ctime = fs_time;

    bio(FS_INO_BLOCK(super, ino), BIO_READ);
    memcpy(block + FS_INO_OFS(super, ino), inode, sizeof(struct fs_inode));
    bio(FS_INO_BLOCK(super, ino), BIO_WRITE);
}

next()
{
    ch = getc(proto_file);
}

/* skip whitespace in the prototype file. */

skip(need_eof)
{
    while (isspace(ch)) {
        if (ch == '\n') line++;
        next();
    }

    if (need_eof && (ch != -1)) error("expected eof");
    if (!need_eof && (ch == -1)) error("premature eof");
}

/* get a number from the prototype file. it's 
   interpreted as decimal, unless prefixed with
   0, in which case it's interpreted as octal. */

get_num()
{
    int i = 0;
    int octal = 0;

    skip(0);
    if (!isdigit(ch)) error("number expected");
    if (ch == '0') ++octal;

    while (isdigit(ch)) {
        if (octal) 
            i <<= 3;
        else
            i *= 10;

        i += ch - '0';
        next();
    }

    return i;
}

/* read a name (any sequence of non-space characters)
   from the prototype file, max 'len'-1 characters. */

get_name(name, len)
    char * name;
{
    skip(0);

    while (!isspace(ch)) {
        if (len == 1) error("name too long");
        *name++ = ch;
        --len;
        next();
    }

    *name = 0;
}

do_proto(dir, dir_ino)
    struct fs_inode * dir;
{
    struct fs_inode   entry_inode;
    int               entry_ino;
    char              entry_name[DIRSIZ + 1];
    int               major;
    int               minor;
    FILE            * fp;
    char              path[64];
    int               i;

    for (;;) {
        get_name(entry_name, sizeof(entry_name));
        if (!strcmp(entry_name, "$")) return 0;

        entry_ino = ialloc(&entry_inode);
        entry(dir, entry_name, entry_ino);

        entry_inode.links = 1;
        entry_inode.mode = get_num();
        entry_inode.uid = get_num();
        entry_inode.gid = get_num();

        if ((entry_inode.mode & S_IFMT) == S_IFDIR) {
            entry(&entry_inode, ".", entry_ino);
            ++(entry_inode.links);
            entry(&entry_inode, "..", dir_ino);
            ++(dir->links);
            do_proto(&entry_inode, entry_ino);
        } else if ((entry_inode.mode & S_IFMT) != S_IFREG) {
            major = get_num();
            minor = get_num();
            entry_inode.size = makedev(major, minor);
        } else {
            get_name(path, sizeof(path));
            fp = fopen(path, "r");
            if (fp == NULL) error("can't open source file");
            while (i = fread(buffer, 1, FS_BLOCK_SIZE, fp)) iwrite(&entry_inode, buffer, i);
            fclose(fp);
        }

        iput(entry_ino, &entry_inode);
    }
}

main(argc, argv)
    char ** argv;
{
    int i;

    fs_time = time(NULL);

    --argc;
    ++argv;

    if ((argc < 2) || (argc > 4)) 
        error("syntax: <device> <# blocks> [ <prototype> ] [ < # inodes> ]");

    device_file = fopen(*argv++, "r+");
    if (device_file == NULL) error("can't open device file");

    nr_blocks = atoi(*argv++);
    if (nr_blocks < 100) error("bad filesystem size");

    if (*argv) {
        proto_file = fopen(*argv, "r");
        if (proto_file) argv++;
    }

    if (*argv) {
        nr_inodes = atoi(*argv);
        if ((nr_inodes < FS_INODES_PER_BLOCK) || (nr_inodes >= nr_blocks))
            error("bad inode count");
    } else 
        nr_inodes = nr_blocks / 2;

    /* compute basic characteristics of the
       file system and fill in the superblock. */

    super.magic = FS_SUPER_MAGIC;
    super.magic2 = FS_SUPER_MAGIC2;
    super.bios_magic = FS_SUPER_BIOS_MAGIC;
    super.ctime = fs_time;
    super.mtime = fs_time;
    super.nr_blocks = nr_blocks;
    super.nr_inodes = ROUND_UP(nr_inodes, FS_INODES_PER_BLOCK);
    super.nr_bmap_blocks = ROUND_UP(nr_blocks, FS_BITS_PER_BLOCK) / FS_BITS_PER_BLOCK;
    super.nr_ino_blocks = super.nr_inodes / FS_INODES_PER_BLOCK;
    super.nr_imap_blocks = ROUND_UP(super.nr_ino_blocks, FS_BITS_PER_BLOCK) / FS_BITS_PER_BLOCK;
    super.nr_free_inodes = super.nr_inodes;
    super.nr_free_blocks = nr_blocks - FS_DATA_START(super);

    /* from here on out, nr_inodes and nr_blocks 
       are used as the number of allocated items */

    nr_inodes = FS_INO_ROOT;
    nr_blocks = FS_DATA_START(super);

    /* build the root directory. */

    ialloc(&root);
    root.mode = S_IFDIR | 0555; 
    root.links = 2;
    entry(&root, ".", FS_INO_ROOT);
    entry(&root, "..", FS_INO_ROOT);

    /* if there's a prototype, populate the filesystem. */

    if (proto_file) {
        line++;
        next();
        do_proto(&root, FS_INO_ROOT);
        skip(1);
        fclose(proto_file);
    }

    iput(FS_INO_ROOT, &root);

    /* write out bitmaps and superblock. */

    bitmap(FS_BMAP_START(super), super.nr_bmap_blocks, nr_blocks);
    bitmap(FS_IMAP_START(super), super.nr_imap_blocks, nr_inodes);

    memset(block, 0, FS_BLOCK_SIZE);
    memcpy(block + FS_SUPER_OFFSET, &super, sizeof(struct fs_super));
    bio(FS_SUPER_LBN, BIO_WRITE);

    return 0;
}
