/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This module is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This module is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this module; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "list.h"
#include "memory.h"
#include "mhandle.h"

struct cuda_memory_object_t *cuda_memory_object_create(void) {
  struct cuda_memory_object_t *mem;

  /* Create memory object */
  mem = xcalloc(1, sizeof(struct cuda_memory_object_t));

  /* Initialize */
  mem->id = list_count(memory_object_list);

  /* Add to memory object list */
  list_add(memory_object_list, mem);

  return mem;
}

void cuda_memory_object_free(struct cuda_memory_object_t *mem) {
  list_remove(memory_object_list, mem);

  free(mem);
}
