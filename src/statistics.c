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

#include <stdio.h>
#include <stdlib.h>
#include "definesizes.h"
#include "sds.h"
#include "input.h"
#include "bitapvec.h"
#include "processor.h"
#include "macros.h"
#include "stack.h"
#include "output.h"


int statistics[max_size_macro + 1];
int macrocounters[max_size_macro + 1];
  

void print_statistics(void)
{
  int i,
      total_count = 0,
      total_calls = 0;
  status_bitap *bitaps;
  pattern_data *arglists;
  
 count_macros(macrocounters);

  
  for(i = 1; i <= max_size_macro; i++)
  {
    printf(" Macro size: %i, number of macros: %i, number of calls: %i\n",i , macrocounters[i], statistics[i]);

    total_calls += statistics[i];
    total_count += macrocounters[i];
  }

  printf(" Total           number of macros: %i, number of calls: %i\n", total_count, total_calls);

  printf("\n Used: %i of total %i in program list\n", end_program_list, size_program_list);

  printf("\n Used: %i of total %i in division list\n\n", end_div_list, size_div_list);
  
  bitaps = first_status_bitap;
  
  while(bitaps != NULL)
  {
    printf("Active macro set: %s\n", bitaps->name);
    bitaps = bitaps->next;
  }

  printf("\n");
  
  arglists = arglast;
  
  while(arglists != NULL)
  {
    printf("Active pattern: %s\n", arglists->name);
    arglists = arglists->prev;
  }

}
