/*
 * Copyright 2025 Marco de Beurs
 * 
 * This file is part of m0.
 * 
 * m0 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 * 
 * m0 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with m0. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "xmalloc.h"

void *xmalloc (size_t size)
{
  void *value = malloc (size);

  if (value == NULL)
  {
    fprintf(stderr, "Error; memory exhausted: %s\n", strerror(errno));
    exit(-1);
  }
  return value;
}


void *xrealloc (void *ptr, size_t size)
{
    // fprintf(stderr, "REalloc ptr: %p  size: %i\n", ptr, size);


  void *value = realloc (ptr, size);

  if (value == NULL)
  {
    fprintf(stderr, "Error; memory exhausted: %s\n", strerror(errno));
    exit(-1);
  }
  return value;
}

void xfree (void *ptr)
{
  free(ptr);

}
