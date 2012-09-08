/*
 * This file is part of fitbitd.
 *
 * fitbitd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fitbitd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fitbitd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"

int b64decode(uint8_t *buf, size_t sz, const unsigned char* str)
{
    const unsigned char *cur, *start;
    int d, dlast, phase;
    size_t rem;
    unsigned char c;
    static int table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };

    dlast = phase = 0;
    start = buf;
    rem = sz;
    for (cur = str; *cur != '\0'; cur++)
    {
        d = table[(int)*cur];
        if(d != -1)
        {
            switch(phase)
            {
            case 0:
                ++phase;
                break;
            case 1:
                if (!rem)
                    return -1;
                c = ((dlast << 2) | ((d & 0x30) >> 4));
                *buf++ = c;
                rem--;
                ++phase;
                break;
            case 2:
                if (!rem)
                    return -1;
                c = (((dlast & 0xf) << 4) | ((d & 0x3c) >> 2));
                *buf++ = c;
                rem--;
                ++phase;
                break;
            case 3:
                if (!rem)
                    return -1;
                c = (((dlast & 0x03 ) << 6) | d);
                *buf++ = c;
                rem--;
                phase = 0;
                break;
            }
            dlast = d;
        }
    }
    return buf - start;
}

int b64encode(uint8_t *buf, size_t buf_sz, const uint8_t* data, size_t data_sz)
{
   const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   size_t buf_idx = 0;
   size_t x;
   uint32_t n = 0;
   int padCount = data_sz % 3;
   uint8_t n0, n1, n2, n3;
 
   /* increment over the length of the string, three characters at a time */
   for (x = 0; x < data_sz; x += 3) 
   {
      /* these three 8-bit (ASCII) characters become one 24-bit number */
      n = data[x] << 16;
 
      if((x+1) < data_sz)
         n += data[x+1] << 8;
 
      if((x+2) < data_sz)
         n += data[x+2];
 
      /* this 24-bit number gets separated into four 6-bit numbers */
      n0 = (uint8_t)(n >> 18) & 63;
      n1 = (uint8_t)(n >> 12) & 63;
      n2 = (uint8_t)(n >> 6) & 63;
      n3 = (uint8_t)n & 63;
 
      /*
       * if we have one byte available, then its encoding is spread
       * out over two characters
       */
      if(buf_idx >= buf_sz) return -1;   /* indicate failure: buffer too small */
      buf[buf_idx++] = base64chars[n0];
      if(buf_idx >= buf_sz) return -1;   /* indicate failure: buffer too small */
      buf[buf_idx++] = base64chars[n1];
 
      /*
       * if we have only two bytes available, then their encoding is
       * spread out over three chars
       */
      if((x+1) < data_sz)
      {
         if(buf_idx >= buf_sz) return -1;   /* indicate failure: buffer too small */
         buf[buf_idx++] = base64chars[n2];
      }
 
      /*
       * if we have all three bytes available, then their encoding is spread
       * out over four characters
       */
      if((x+2) < data_sz)
      {
         if(buf_idx >= buf_sz) return -1;   /* indicate failure: buffer too small */
         buf[buf_idx++] = base64chars[n3];
      }
   }  
 
   /*
    * create and add padding that is required if we did not have a multiple of 3
    * number of characters available
    */
   if (padCount > 0) 
   { 
      for (; padCount < 3; padCount++) 
      { 
         if(buf_idx >= buf_sz) return -1;   /* indicate failure: buffer too small */
         buf[buf_idx++] = '=';
      } 
   }
   if(buf_idx >= buf_sz) return -1;   /* indicate failure: buffer too small */
   buf[buf_idx] = 0;
   return 0;
}
