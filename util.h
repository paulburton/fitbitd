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

#ifndef __util_h__
#define __util_h__

#if DEBUG == 1
  #include <assert.h>
  #define ASSERT(x) do { assert(x); } while (0)
#else
  #define ASSERT(x)
#endif

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof((a)[0]))

#define CHAINERR_LTZ(x, o) do { \
    int __ret; \
    __ret = x; \
    if (__ret < 0) { \
        ERR("failed %s:%d %d %s\n", __FILE__, __LINE__, __ret, #x); \
        goto o; \
    } \
} while (0)

#define CHAINERR_NULL(v, x, o) do { \
    v = x; \
    if (!v) { \
        ERR("failed %s:%d %s\n", __FILE__, __LINE__, #x); \
        goto o; \
    } \
} while (0)

#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#endif /* __util_h__ */
