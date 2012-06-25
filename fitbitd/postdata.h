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

#ifndef __postdata_h__
#define __postdata_h__

typedef struct postdata_s postdata_t;

postdata_t *postdata_create(void);
void postdata_destroy(postdata_t *pd);
int postdata_append(postdata_t *pd, const char *name, const char *val);
char *postdata_string(postdata_t *pd);

#endif /* __postdata_h__ */
