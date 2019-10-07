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

#include "../include/stdarg.h"

/*
 * driver for 6845-based (CGA/EGA/VGA) 80x25 console.
 * ultimate aims to be an enhanced VT-52 compatible.
 */

#define COLS 80
#define ROWS 25

static unsigned short *vram = (unsigned short *) 0xb8000;

#define CRTC_INDEX          0x3d4
#define CRTC_DATA           0x3d5

#define REG_CURSOR_START    0x0a
#define REG_CURSOR_END      0x0b
#define REG_CURSOR_ADDRHI   0x0e
#define REG_CURSOR_ADDRLO   0x0f

#define NORMAL_ATTR         0x1700      /* grey on blue .. */
#define INVERSE_ATTR        0x7100      /* .. blue on grey */

static unsigned cursor;         /* (linear) cursor position */
static unsigned short attr;     /* current attribute */

static
write_crtc(reg, val)
{
    outb(CRTC_INDEX, reg);
    outb(CRTC_DATA, val);
}

static
read_crtc(reg)
{
    outb(CRTC_INDEX, reg);
    return inb(CRTC_DATA);
}

/*
 * send output to console, processing control characters and escape sequences.
 */

static
putc(c)
{
    switch (c)
    {
    case 8: /* backspace */
        if (cursor % COLS) --cursor;
        break;
    case '\n':
        cursor += COLS;
        break;
    case '\r':
        cursor -= (cursor % COLS);
        break;
    default:
        vram[cursor++] = c | attr;
    }

    if (cursor >= (ROWS * COLS)) {  /* upward scroll */
        int i;

        for (i = COLS; i < (COLS * ROWS); ++i)
            vram[i - COLS] = vram[i];

        for (i = (COLS * (ROWS - 1)); i < (COLS * ROWS); ++i)
            vram[i] = ' ' | attr;

        cursor -= COLS;
    }

    write_crtc(REG_CURSOR_ADDRHI, cursor >> 8);
    write_crtc(REG_CURSOR_ADDRLO, cursor);
}

/*
 * print an unsigned number 'n' to the console in the given 'base' (10 or 16)
 */

#define MAX_DIGITS 20   /* a 64-bit number has at most 20 decimal digits */

static char digits[] = "0123456789abcdef";

static
printn(n, base)
unsigned long n;
{
    static char buf[MAX_DIGITS];
    int pos = 0;

    do {
        buf[pos++] = digits[n % base];
        n /= base;
    } while (n);

    while (pos) putc(buf[--pos]);
}

/*
 * the usual printf()-like kernel function for console messages.
 */

printf(fmt)
char *fmt;
{
    va_list args;
    char *s;
    long n;

    va_start(args, fmt);

    while (*fmt)
    {
        if (*fmt == '%') {
            ++fmt;
            switch (*fmt)
            {
            case 'd':
                n = va_arg(args, int);
                if (n < 0) {
                    putc('-');
                    n = -n;
                }
                printn(n, 10);
                break;
            case 'x':
                n = va_arg(args, unsigned);
                printn(n, 16);
                break;
            case 's':
                s = va_arg(args, char *);
                while (*s) putc(*s++);
                break;
            default:
                putc(*fmt);
            }
        } else {
            if (*fmt == '\n') putc('\r');
            putc(*fmt);
        }

        ++fmt;
    }

    va_end(args);
}

/*
 * called from main() to enable early console output.
 */

cons_init()
{
    int i;
    
    /*
     * make the cursor a block and note its current position.
     */

    cursor = read_crtc(REG_CURSOR_ADDRHI);
    cursor <<= 8;
    cursor += read_crtc(REG_CURSOR_ADDRLO);

    write_crtc(REG_CURSOR_START, 0);
    write_crtc(REG_CURSOR_END, 15);

    /*
     * normalize the screen attributes
     */

    attr = NORMAL_ATTR;

    for (i = 0; i < (ROWS * COLS); ++i)
        vram[i] = (vram[i] & 0x00FF) | attr;
}

/* vi: set ts=4 expandtab: */
