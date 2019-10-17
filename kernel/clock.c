/* Copyright (c) 2019 Charles E. Youse (charles@gnuless.org).
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

#include "../include/sys/types.h"
#include "../include/sys/clock.h"

time_t time;        /* current time of day */

/* the CMOS/NVRAM/RTC is a 256-byte address space accessed
   via an index register and a data window. */

#define NVRAM_INDEX 0x70        /* index register */
#define NVRAM_DATA  0x71        /* data window */

static
nvram(reg)
{
    outb(NVRAM_INDEX, reg | 0x80);  /* high bit set = NMI disabled */
    return inb(NVRAM_DATA);
}

/* take a consistent snapshot of the RTC state. in order to do this, we
   wait for the clock to tick over, so this can take up to a second. */

struct rtc
{
    unsigned char second,
                  minute,
                  hour,
                  day,
                  month,
                  year,
                  status_a,
                  status_b;
};

#define RTC_STATUS_A_BUSY   0x80    /* update in progress */
#define RTC_STATUS_B_24HR   0x02    /* clock is 24-hr mode */
#define RTC_STATUS_B_BIN    0x01    /* binary mode (vs. BCD) */

static
rtc_read(rtc)
struct rtc *rtc;
{
    /* these registers must agree with offsets into 'struct rtc' */

    static char regs[] = { 0x00, 0x02, 0x04, 0x07, 0x08, 0x09, 0x0a, 0x0b };
    char *cp = (char *) rtc;
    int i;

    /* synchronize to the clock by waiting for an update
       to start, and then wait for it to complete. */

    while (!(nvram(0x0a) & RTC_STATUS_A_BUSY)) ;
    while (nvram(0x0a) & RTC_STATUS_A_BUSY) ;

    for (i = 0; i < sizeof(regs); ++i) *cp++ = nvram(regs[i]);
}

/* Howard Hinnant's [public-domain] algorithm to get days offset from epoch */

static
hinnant(y, m, d)
{
    unsigned yoe;
    unsigned doy;
    unsigned doe;
    int era;

    y -= (m <= 2);
    era = ((y >= 0) ? y : (y - 399)) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + ((m > 2) ? -3 : 9)) + 2)/5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return era * 146097 + ((int) doe) - 719468;
}

/* interpret an 8-bit BCD value */

static
bcd(v)
{
    return (((v >> 4) & 0x0F) * 10) + (v & 0x0F);
}

/* returns the current UNIX epoch time, as read from the RTC. note that
   since we read the RTC, this can busy-wait up to a second. the upshot
   is that consecutive calls to epoch() will return almost exactly one
   second apart, a "feature" we use to tune the scheduling timers. */

time_t
epoch()
{
    time_t time;
    int pm;
    struct rtc rtc;

    rtc_read(&rtc);

    /* the RTC is either a 12-hr or 24-hr, and either
       BCD or binary. normalize it to 24-hr binary. */
 
    if (rtc.status_b & RTC_STATUS_B_24HR)
        pm = 0;
    else {
        pm = rtc.hour & 0x80;
        rtc.hour &= 0x7F;
    }

    if (!(rtc.status_b & RTC_STATUS_B_BIN)) {
        rtc.second = bcd(rtc.second);
        rtc.minute = bcd(rtc.minute);
        rtc.hour = bcd(rtc.hour);
        rtc.day = bcd(rtc.day);
        rtc.month = bcd(rtc.month);
        rtc.year = bcd(rtc.year);
    }

    if (pm) rtc.hour = (rtc.hour + 12) % 24;

    /* now convert to time in seconds since January 1, 1970 0000 GMT */

    time = hinnant(rtc.year + 2000, rtc.month, rtc.day);
    time *= 86400;
    time += rtc.hour * 3600;
    time += rtc.minute * 60;
    time += rtc.second;

    return time;
}

/* vi: set ts=4 expandtab: */
