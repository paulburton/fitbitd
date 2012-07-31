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

#ifndef __log_h__
#define __log_h__

#include <stdio.h>

#ifndef LOG_TAG
#define LOG_TAG __FILE__
#endif

#define ERR(x...) do { fprintf(stderr, "E:" LOG_TAG ": " x); fflush(stderr); } while (0)
#define INFO(x...) do { fprintf(stderr, "I:" LOG_TAG ": " x); fflush(stderr); } while (0)

#if DEBUG == 1
#define DBG(x...) do { fprintf(stderr, "D:" LOG_TAG ": " x); fflush(stderr); } while (0)
#else
#define DBG(x...)
#endif

#endif /* __log_h__ */
