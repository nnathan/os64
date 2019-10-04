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
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include "../include/fs.h"

char            * boot_path = "/lib/boot.bin";
char            * device_path; 
int               fd;
struct fs_super   super;
char              block[FS_BLOCK_SIZE];

error(msg)
    char * msg;
{
    fprintf(stderr, "mkboot: %s\n", msg);
    exit(1);
}

main(argc, argv)
    char ** argv;
{
    int opt;

    while ((opt = getopt(argc, argv, "b:")) != -1) {
        switch (opt)
        {
        case 'b':
            boot_path = optarg;
            break;
        default:
            exit(1);
        }
    }    

    device_path = argv[optind++];
    if (!device_path || argv[optind]) error("syntax");
    
    /* read boot block */    

    fd = open(boot_path, 0);
    if (fd == -1) error("can't open boot block");
    if (read(fd, block, FS_BLOCK_SIZE) != FS_BLOCK_SIZE) 
        error("can't read boot block");
    close(fd);

    /* read super block */

    fd = open(device_path, 2);
    if (fd == -1) error("can't open device");
    lseek(fd, (long) FS_SUPER_OFFSET, SEEK_SET);
    if (read(fd, &super, sizeof(super)) != sizeof(super))
        error("can't read superblock");
    
    if (    (super.magic != FS_SUPER_MAGIC)
        ||  (super.magic2 != FS_SUPER_MAGIC2)
        ||  (super.bios_magic != FS_SUPER_BIOS_MAGIC) )
    {
        error("invalid superblock");
    }

    lseek(fd, 0L, SEEK_SET);
    memcpy(block + FS_SUPER_OFFSET, &super, sizeof(super));
    if (write(fd, block, FS_BLOCK_SIZE) != FS_BLOCK_SIZE) 
        error("can't write to device");

    close(fd);      
    exit(0);
}
