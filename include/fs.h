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

#ifndef _FS_H
#define _FS_H

/* the os/64 filesystem is conventional with 4K blocks.
   the first block is the superblock, which is shared with 
   the boot code, followed by 'nr_bmap_blocks' block bitmaps
   (1 = free block, 0 = used), followed by 'nr_imap_blocks'
   inode bitmaps, followed by 'nr_inodes_blocks' of inodes. */

#define FS_BLOCK_SIZE       4096            /* 4K blocks */
#define FS_BLOCK_SHIFT      12              /* log2(FS_BLOCK_SIZE) */

/* the superblock data starts at offset FS_SUPER_OFFSET in block 0. */

#define FS_SUPER_LBN        0L
#define FS_SUPER_OFFSET     384     /* currently bytes 384-512 */

/* designated inode numbers (ino_t) */

#define FS_INO_NONE         0    /* ino_t 0 means "none" */
#define FS_INO_ROOT         1    /* so root directory is ino_t 1 */

struct fs_super
{
    unsigned        magic;              /* FS_SUPER_MAGIC */
    int             reserved0;
    time_t          ctime;              /* creation time */
    time_t          mtime;              /* last mount time */
    unsigned        nr_bmap_blocks;     /* number of block map blocks */
    unsigned        nr_imap_blocks;     /* number of inode map blocks */
    unsigned        nr_ino_blocks;      /* number of inode blocks */
    int             reserved1;
    daddr_t         nr_blocks;          /* total blocks */
    daddr_t         nr_free_blocks;     /* number free */
    ino_t           nr_inodes;          /* total inodes */
    ino_t           nr_free_inodes;     /* number free */
    char            reserved2[60];
    short           magic2;             /* FS_SUPER_MAGIC2 */
    short           bios_magic;         /* 0xAA55 for BIOS */
};

#define FS_SUPER_MAGIC      0xABE01E50
#define FS_SUPER_MAGIC2     ((short) 0x87CD)
#define FS_SUPER_BIOS_MAGIC ((short) 0xAA55)

/* starting blocks for various regions */

#define FS_BMAP_START(fs)   (1)       
#define FS_IMAP_START(fs)   (FS_BMAP_START(fs) + ((fs).nr_bmap_blocks))
#define FS_INO_START(fs)    (FS_IMAP_START(fs) + ((fs).nr_imap_blocks))
#define FS_DATA_START(fs)   (FS_INO_START(fs) + ((fs).nr_ino_blocks))

/*
 * os/64 disk indoes are similarly very conventional
 */

#define FS_INODE_BLOCKS     11      /* number of blocks in fs_inode */
#define FS_INODE_NR_DIRECT  8       /* how many of those are direct */
#define FS_INODE_INDIRECT   8       /* index of indirect block */
#define FS_INODE_DOUBLE     9       /* index of double-indirect block */
#define FS_INODE_TRIPLE     10      /* index of triple-indirect block */

struct fs_inode
{
    mode_t  mode;
    nlink_t links;
    uid_t   uid;
    gid_t   gid;
    off_t   size;           /* or dev, if S_IFBLK or S_IFCHR */
    time_t  atime;
    time_t  ctime;
    time_t  mtime;
    daddr_t blocks[FS_INODE_BLOCKS];
};

#define FS_INODES_PER_BLOCK     (FS_BLOCK_SIZE / sizeof(struct fs_inode))
#define FS_BLOCKS_PER_BLOCK     (FS_BLOCK_SIZE / sizeof(long))
#define FS_BITS_PER_BLOCK       (FS_BLOCK_SIZE * 8)

/* number of bytes mapped per direct block, per indirect block, ... */

#define FS_DIRECT_BYTES         (FS_INODE_NR_DIRECT * FS_BLOCK_SIZE)
#define FS_INDIRECT_BYTES       (((long) FS_BLOCKS_PER_BLOCK) * FS_BLOCK_SIZE)
#define FS_DOUBLE_BYTES         (((long) FS_BLOCKS_PER_BLOCK) * FS_INDIRECT_BYTES)
#define FS_TRIPLE_BYTES         (((long) FS_BLOCKS_PER_BLOCK) * FS_DOUBLE_BYTES)

#define FS_MAX_FILE_SIZE        (FS_DIRECT_BYTES + FS_INDIRECT_BYTES + FS_DOUBLE_BYTES + FS_TRIPLE_BYTES)

/* the block and offset in that block where an inode is found */

#define FS_INO_BLOCK(fs,i)  (((((long) i) * sizeof(struct fs_inode)) / FS_BLOCK_SIZE) + FS_INO_START(fs))
#define FS_INO_OFS(fs,i)    ((((long) i) * sizeof(struct fs_inode)) % FS_BLOCK_SIZE)

#endif /* _FS_H */
