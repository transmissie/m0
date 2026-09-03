/*
 * Copyright 2025, 2026 Marco de Beurs
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
#include "statistics.h"


int statistics[max_size_macro + 1];
int macrocounters[max_size_macro + 1];
int max_position = size_reduce_history_chars;  

int stat_vml = 0,
    stat_vml_max_len = 0;

    
void print_macros_lists(wordlist *words)
{
  int i,
      macro_index;
      
  do
    {
      for(i = 0; i < words->num_words; i++)
      {
        if(words->word_length[i] != 0)
        {
          macro_index = words->macro[i];
          printf(" name:%s,def:", macro_list[macro_index].name);
          if(macro_list[macro_index].def_len != 0)
          {
            printf("%s\n", macro_list[macro_index].def);
          }
          else
          {
            printf("--EMPTY--\n");
          }
        }
      } 

      words = words->next;
      
    } while(words != NULL);
}

void print_macros_pat(pattern_data *pat)
{
  int i,
      j,
      macro_index;
  pattern_masks *masks;

  if(pat != NULL)
  {
    for(i = 0; i < pat->vec_size; i++)
    {
      masks = pat->masks;
      for(j = 0; j < masks->masks_end; j++)
      {
        macro_index = masks->masks_run[j];
        if(macro_index >= 0)
        {
          printf(" name:%s,def:", macro_list[macro_index].name);
          if(macro_list[macro_index].def_len != 0)
          {
            printf("%s\n", macro_list[macro_index].def);
          }
          else
          {
            printf("--EMPTY--\n");
          }
        }
        else
        {
          printf(" part name:%s\n", macropart_list[-macro_index].name);
        }
        
      }
    }
  }
}

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

  printf("\n Maximum length of vlm macros: %i, number of calls: %i\n", stat_vml_max_len, stat_vml);

  printf("\n Used: %i of total %i in program list\n", end_program_list, size_program_list);

  printf("\n Used: %i of total %i in division list\n\n", end_div_list, size_div_list);
  
  bitaps = first_status_bitap;
  
  while(bitaps != NULL)
  {
    printf("\n Active macro set: %s\n", bitaps->name);
    printf(" Defined macros:\n");
    
    print_macros_lists(bitaps->word15);
    print_macros_lists(bitaps->word64);
    print_macros_pat(bitaps->patlist);
    
    bitaps = bitaps->next;
  }
  
  printf("\n");
  
  arglists = arglast;
  
  while(arglists != NULL)
  {
    if(sdslen(arglists->name) != 0)
    {  
      printf(" Active pattern: %s, used positions: %i\n", arglists->name, arglists->end);
    }
    else
    {
      printf(" Vlm pattern is active, used positions: %i\n", arglists->end);
    }      

    arglists = arglists->prev;
  }

  printf("\n Maximum of history buffer: %i, with reduce limit: %i\n", max_position, size_reduce_history_chars);

}
