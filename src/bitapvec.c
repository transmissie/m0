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
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "exitcodes.h"
#include "definesizes.h"
#include "input.h"
#include "xmalloc.h"
#include "sds.h"
#include "bitapvec.h"
#include "processor.h"
#include "macros.h"



vectors  (*current_vec);


/* The different sizes of the search string are placed on fixed positions.
 * In the first vector are placed the strings with length: 1, 3, 5, 7, 9, 11, 13 and 15.
 * In the second vector: 2, 4, 6, 8, 10, 12 and 14.
 * The second vector has 8 bits left used as following:
 * 2 bits for checking simple arguments strings:
 * arg + number / all / all quoted / amount of args
 * 3 bits for checking stacks arguments strings:
 * arg + stack + number / all / all quoted / amount of args
 * These are checked using the fourth init and mask vector
 * The third vector is used for strings longer than 15.
 */

const uint64_t init_vector[4] =  /* the bit pattern to use for initialising in the bitap algo */
{
/* bit  64  60        50        40        30        20        10       1 */
      0b0000000000000010000000000001000000000010000000010000001000010011,
      0b0000000000000000000001000000000001000000000100000001000001000101,
      0b0000000000000000000000000000000000000000000000000000000000000001,  
      0b0000010100000000000000000000000000000000000000000000000000000000
};

const uint64_t mask_vector[4]=  /* the bit pattern to use for checking the result in the bitap algo */
{
/* bit  64  60        50        40        30        20        10       1 */
      0b1000000000000001000000000000100000000001000000001000000100001001,
      0b0000000010000000000000100000000000100000000010000000100000100010,
      0b1111111111111111111111111111111111111111111111111000000000000000,  
      0b0001001000000000000000000000000000000000000000000000000000000000
};

const uint64_t arg_vector[2]=  /* the bit pattern to use for checking the arguments */
{
/* bit  64  60        50        40        30        20        10       1 */
      0b0000001000000000000000000000000000000000000000000000000000000000,  /* simple arg  */
      0b0001000000000000000000000000000000000000000000000000000000000000  /* stacks arg */
};


/* array with info to select the correct vector when the length of the word is known */
const int l_to_vec[16] = 
/*  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 */
  {-1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};

/* array with masks for the first bit for the first 2 vectors */
const uint64_t mask_first [16] =
{  0,
   0b0000000000000000000000000000000000000000000000000000000000000001,  /* 1 */
   0b0000000000000000000000000000000000000000000000000000000000000001,  /* 2 */
   0b0000000000000000000000000000000000000000000000000000000000000010,  /* 3 */
   0b0000000000000000000000000000000000000000000000000000000000000100,  /* 4 */
   0b0000000000000000000000000000000000000000000000000000000000010000,  /* 5 */
   0b0000000000000000000000000000000000000000000000000000000001000000,  /* 6 */
   0b0000000000000000000000000000000000000000000000000000001000000000,  /* 7 */
   0b0000000000000000000000000000000000000000000000000001000000000000,  /* 8 */
   0b0000000000000000000000000000000000000000000000010000000000000000,  /* 9 */
   0b0000000000000000000000000000000000000000000100000000000000000000,  /* 10 */
   0b0000000000000000000000000000000000000010000000000000000000000000,  /* 11 */
   0b0000000000000000000000000000000001000000000000000000000000000000,  /* 12 */
   0b0000000000000000000000000001000000000000000000000000000000000000,  /* 13 */
   0b0000000000000000000001000000000000000000000000000000000000000000,  /* 14 */
   0b0000000000000010000000000000000000000000000000000000000000000000   /* 15 */
};
  
  
/* array with masks for selecting the correct result bit from the first 2 vectors */
const uint64_t mask_length [16] =
{  0,
   0b0000000000000000000000000000000000000000000000000000000000000001,  /* 1 */
   0b0000000000000000000000000000000000000000000000000000000000000010,  /* 2 */
   0b0000000000000000000000000000000000000000000000000000000000001000,  /* 3 */
   0b0000000000000000000000000000000000000000000000000000000000100000,  /* 4 */
   0b0000000000000000000000000000000000000000000000000000000100000000,  /* 5 */
   0b0000000000000000000000000000000000000000000000000000100000000000,  /* 6 */
   0b0000000000000000000000000000000000000000000000001000000000000000,  /* 7 */
   0b0000000000000000000000000000000000000000000010000000000000000000,  /* 8 */
   0b0000000000000000000000000000000000000001000000000000000000000000,  /* 9 */
   0b0000000000000000000000000000000000100000000000000000000000000000,  /* 10 */
   0b0000000000000000000000000000100000000000000000000000000000000000,  /* 11 */
   0b0000000000000000000000100000000000000000000000000000000000000000,  /* 12 */
   0b0000000000000001000000000000000000000000000000000000000000000000,  /* 13 */
   0b0000000010000000000000000000000000000000000000000000000000000000,  /* 14 */
   0b1000000000000000000000000000000000000000000000000000000000000000   /* 15 */
};



wordlist (*current_word15);

wordlist (*current_word64);


pattern_data *arglist,
             *arglast = NULL;

status_pattern *current_status_pattern;

status_bitap   *first_status_bitap = NULL,
               *last_status_bitap = NULL,
               *current_status_bitap = NULL;

/* chars used for filling arguments in macro definition */
argument_chars *current_arg_chars;


  
  
argument_chars *init_arg_chars(uint8_t first, uint8_t all, uint8_t allq, uint8_t num, uint8_t firstalt)
{
  argument_chars *new;
  
  new = xmalloc(sizeof(argument_chars));
  
  new->first = first;
  new->all = all;
  new->allq = allq;
  new->num = num;
  new->firstalt = firstalt;

  return(new);
}



wordlist *init_wordlist(int size)
{
  wordlist *new;
  size_t total;
  
  total = sizeof(wordlist)+sizeof(uint64_t[size][size_index]);

  if(debug)
  {
    printf("\n\n >> size of wordlist: %li\n", total);
  }
  
  int i, j;
  
  new = xmalloc(total);
  
  /* clear everything */
  for(j=0; j<size; j++)
  {
    for(i=0; i<size_index; i++)
    {
      new->vecs[j][i] = 0LL;
    }
  }
  for(i=0; i<64; i++)
  {
    new->word_length[i] = 0;
    new->macro[i] =  0;
  }
  new->num_words = 0;
  new->next = NULL;
  new->word_size = size;
  
  return(new);

}

vectors *init_vectors(void)
{
  vectors *new;
  int i, j;
  
  new = xmalloc(sizeof(vectors));
  
  /* clear everything */
  for(j=0; j<3; j++)
  {
    for(i=0; i<size_index; i++)
    {
      (*new)[j][i] = 0LL;
    }
  }
 
  return(new);
}

status_bitap *new_vectorset(uint8_t *setname, int length)
{
  status_bitap   *new_status_bitap;
  
  /* set current status */
  new_status_bitap = xmalloc(sizeof(status_bitap));
  
  new_status_bitap->vec = init_vectors();
  new_status_bitap->word64 = init_wordlist(64);
  new_status_bitap->word15 = init_wordlist(15);
  new_status_bitap->patlist = NULL; /* initially not used */
  new_status_bitap->argchars = init_arg_chars('$', '*', '@', '#', '$');
  new_status_bitap->num_digits = 1;
  new_status_bitap->quote_var_start = 1;
  new_status_bitap->quote_var_end = 2;
  new_status_bitap->quote_var_separator = 3;
  new_status_bitap->prev = last_status_bitap;
  new_status_bitap->next = NULL;
  new_status_bitap->name = sdsnewlen(setname, length);

  add_arg_to_vectors(new_status_bitap->vec, new_status_bitap->argchars);

  init_variable(1);
  init_variable(2);
  init_variable(3);
  
  if(new_status_bitap->prev != NULL)
  {
    last_status_bitap->next = new_status_bitap;
  
    last_status_bitap = new_status_bitap;
  }
  
  if(new_status_bitap->prev == NULL)
  {
    last_status_bitap = new_status_bitap;
   
    first_status_bitap = new_status_bitap;
  }
  
  return(new_status_bitap);
}


status_bitap *find_vectorset(uint8_t *setname, int length)
{
  status_bitap *ret;
  sds name;
  int found = -1;


  name = sdsnewlen(setname, length);
  
  ret = first_status_bitap;
  
  while((ret != NULL) && (found != 0))
  {
   found = sdscmp(name, ret->name);
    if(found != 0)
    {
      ret = ret->next;
    }
  }
  
  sdsfree(name);

  return(ret);
}
    

status_bitap *find_or_new_vectorset(uint8_t *setname, int length)
{
  status_bitap *vecset;
  
  vecset = find_vectorset(setname, length);
  
  if(vecset == NULL)
  {
    vecset = new_vectorset(setname, length);
  }
  
  return(vecset);
}
    
void select_vectorset(uint8_t *setname, int length)
{
  status_bitap *new;
  
  new = find_vectorset(setname, length);
  
  if(new != NULL)
  {
    current_status_bitap = new;
    
    current_vec = new->vec;
    current_word64 = new->word64;
    current_word15 = new->word15;
    current_arg_chars = new->argchars;
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; Can not select %*.*s: no such set exists.\n", line_counter, current_input_file_buffer->filename, local_line_counter, length, length, setname);
    exit_code = Exit_user;
  }  
    
}

void set_vectorset(status_bitap *new)
{
  
  if(new != NULL)
  {
    current_status_bitap = new;
    
    current_vec = new->vec;
    current_word64 = new->word64;
    current_word15 = new->word15;
    current_arg_chars = new->argchars;
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; Can not select macro set: no such set exists.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user;
  }  
    
}


void clear_patternvector(pattern_data (*pat_dat), int from)
{
  int i,
      j;
  pattern_vectors (*vec);
  pattern_masks *masks;

  
  vec = pat_dat->vec;
  masks = pat_dat->masks;
  
  /* clear everything */
  for(j = from; j < pat_dat->vec_size; j++)
  {
    for(i = 0; i < size_index; i++)
    {
      (*vec)[j][i] = 0ULL;
    }
    masks[j].init = 0ULL;
    masks[j].mask = 0ULL;
    masks[j].starmask = 0ULL;
    masks[j].zeromask = 0ULL;
    
    
    masks[j].masks_end = 0;
    // masks[j].onetimemasks_end = 0;
  }
  
  /* only if clearing from begin then the whole pattern is cleared */
  if(from == 0)
  {
    pat_dat->end = 0;
  }
}

void resize_pattern(pattern_data (*pat_dat), int add)
{
  int newsize,
      oldsize;
  pattern_vectors (*vec);
  
  newsize = pat_dat->vec_size + add;
  
  pat_dat->vec = xrealloc(pat_dat->vec, newsize * sizeof((*vec)[0]));
  
  pat_dat->masks = xrealloc(pat_dat->masks, newsize * sizeof(pattern_masks));

  oldsize = pat_dat->vec_size;
  
  pat_dat->vec_size = newsize;

  clear_patternvector(pat_dat, oldsize);

}
  

void copy_patternvector(pattern_data (*vec_from), pattern_data (*vec_to))
{
  pattern_vectors (*vec);

  vec_to->end = vec_from->end;

  /* reserve additional new space */
  if(vec_from->vec_size > vec_to->vec_size)
  {
    resize_pattern(vec_to, vec_from->vec_size - vec_to->vec_size);
  }
  else
  {
    vec_to->vec_size = vec_from->vec_size;
  }
  /* and copy data */
  memcpy(vec_to->masks, vec_from->masks, vec_to->vec_size * sizeof(pattern_masks));
  
  memcpy(vec_to->vec, vec_from->vec, vec_to->vec_size * sizeof((*vec)[0]));
  
}


pattern_data *init_patternvectors(uint8_t *patternname, int length)
{
  pattern_data *new;
  pattern_vectors (*vec);
  
  
  new = xmalloc(sizeof(pattern_data));
  
  new->vec = xmalloc(sizeof((*vec)[0]));

  new->masks = xmalloc(sizeof(pattern_masks));
  
  new->vec_size = 1;
  
  /* clear everything */
  clear_patternvector(new, 0);
  
  new->name = sdsnewlen(patternname, length);
  
  new->prev = arglast;
  
  arglast = new;
  
  return(new);
}


pattern_data *find_patternvec(uint8_t *patternname, int length)
{
  pattern_data *find;
  int found;
  sds name;
  
  name = sdsnewlen(patternname, length);
  
  find = arglast;
  found = 0;
  
  while((find != NULL) && (found == 0))
  {
    if(sdscmp(name, find->name) == 0)
    {
      found = 1;
    }
    else
    {
      find = find->prev;
    }
  }
  
  sdsfree(name);
  
  return(find);
}


void add_to_patternvector(pattern_data (*pat_dat), int pattern, int program, int level, pattern_append_option append)
{
  int i,
      j,
      bit_counter,
      pattern_space,
      pattern_len,
      inc_size;
  uint64_t set_bit_mask;
  pattern_vectors (*vec);
  pattern_masks *masks;

 
  j = 0;
  
  /* first entry in list is a start (not checked) with a length of the pattern in the list */
  pattern_len = (charstr[pattern]).data.size;
  
  pattern_space = 64 * pat_dat->vec_size - pat_dat->end;

  if (pattern_len > pattern_space)
  {
      inc_size = pattern_len / 64 + 1; /* only in extreme case should size increase more than 1 */
      
      resize_pattern(pat_dat, inc_size);
  }
  
  if (pattern_len < 1)
  {
      fprintf(stderr, "Error line: %i in file: %s line: %i; length of pattern is 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
  }
  else
  {
    vec = pat_dat->vec;
    masks = pat_dat->masks;
    
    /* set mask */
    bit_counter = pat_dat->end % 64;
    set_bit_mask = 0b1LL << bit_counter;
    
    j = pat_dat->end / 64;

    
    if(append == pattern_no_append)
    {
      /* the first position is set in the init vector */  
      masks[j].init |= set_bit_mask;
    }
    
    /* next entry should have the first character data */
    pattern++;
    
    
    /* go through the list up to the end */
    while((charstr[pattern]).data.type != charrtype_end)
    {
      /* set the bits in the vectors */
      for(i = (charstr[pattern]).data.start; i <= (charstr[pattern]).data.end; i++)
      {
        (*vec)[j][i] |= set_bit_mask;
      }
      
      /* is this a one or more character or zero or more? write star mask */
      if(((charstr[pattern]).data.type == charrtype_oneormoreincr) || ((charstr[pattern]).data.type == charrtype_zeroormoreincr))
      {
        masks[j].starmask |= set_bit_mask;
      }  

      /* is this zero or more? or zero or one? write zero mask */
      if(((charstr[pattern]).data.type == charrtype_zeroormoreincr) || (charstr[pattern]).data.type == charrtype_zerooroneincr)
      {
        masks[j].zeromask |= set_bit_mask;
      }  

      /* is this a trigger character? write trigger mask */
      if((charstr[pattern]).data.type == charrtype_trigincr)
      {
        if(masks[j].masks_end < pattern_size_masks)
        {
          masks[j].mask |= set_bit_mask;
          masks[j].masks[masks[j].masks_end] = set_bit_mask;
          masks[j].masks_run[masks[j].masks_end] = program;
          masks[j].masks_run_patlen[masks[j].masks_end] = -pattern_len;  /* negative length for triggers */
          masks[j].masks_run_level[masks[j].masks_end] = level;
      
          masks[j].masks_end++;
        }
      }  
      
      /* next is end? then write result mask */
      if(((charstr[pattern + 1]).data.type == charrtype_end) && ((charstr[pattern]).data.type != charrtype_trigincr) )
      {
        if(masks[j].masks_end < pattern_size_masks)
        {
          masks[j].mask |= set_bit_mask;
          masks[j].masks[masks[j].masks_end] = set_bit_mask;
          masks[j].masks_run[masks[j].masks_end] = program;
          masks[j].masks_run_patlen[masks[j].masks_end] = pattern_len;
          masks[j].masks_run_level[masks[j].masks_end] = level;
          masks[j].masks_end++;
        }
      }  
      
      /* go to next char in vector ? */
      if((charstr[pattern]).data.size == 1)
      {
        bit_counter++;
        if(bit_counter > 63)
        {
          bit_counter = 0;
          j++;
          set_bit_mask = 0b1LL;
        }
        else
        {
          set_bit_mask <<= 1;
        }

      }       
      
      pattern++;
      
    };
  }
  
  pat_dat->end += pattern_len;
  
}


void add_to_vectors2(vectors (*vec), int macro_name)
{
   int i,
       vec_index;
   uint64_t set_bit_mask;
   
   int macro_name_len;
   
   /* first entry in list is a start (not checked) with a length of the name in the list */
   macro_name_len = (charstr[macro_name]).data.size;
   
   if ((macro_name_len < 1) || (macro_name_len > 64))
   {
        fprintf(stderr, "Error line: %i in file: %s line: %i; length: %i of macro name wrong.\n ", line_counter, current_input_file_buffer->filename, local_line_counter, macro_name_len);
   }
   else
   {
     /* select correct masks */
     if ((macro_name_len >= 1) && (macro_name_len <= 15))
     {     
       set_bit_mask = mask_first[macro_name_len];
       vec_index = l_to_vec[macro_name_len];
     }
     else
     {     
       set_bit_mask = 0b1LL;
       vec_index = 2;
     }
 
     /* next entry should have the first character data */
     macro_name++;
     
     /* go through the list up to the end */
     while((charstr[macro_name]).data.type != charrtype_end)
     {
       /* set the bits in the vectors */
       for(i = (charstr[macro_name]).data.start; i <= (charstr[macro_name]).data.end; i++)
       {
         (*vec)[vec_index][i] |= set_bit_mask;
       }
       
       /* go to next char in vector ? */
       if((charstr[macro_name]).data.size == 1)
       {
         set_bit_mask <<= 1;
       }       
       
       macro_name++;
     };
   }
  
}


void add_arg_to_vectors(vectors (*vec), argument_chars *arg)
{
   int i;
   uint64_t set_bit_mask,
            clear_mask;

   set_bit_mask = 0b1LL << 56;

   clear_mask = ~(0b11111LL << 56);

   /* first clear all bits */
   for(i = 0; i < 256 ; i++)
   {
     (*vec)[1][i] &= clear_mask;
   }


   /* simple arg style */
   (*vec)[1][arg->first] |= set_bit_mask;
   set_bit_mask <<= 1;

   for(i = '0'; i <= '9' ; i++) /* for the different chars for the numbers */
   {
     (*vec)[1][i] |= set_bit_mask;
   }
   (*vec)[1][arg->all] |= set_bit_mask;  /* all args */
   (*vec)[1][arg->allq] |= set_bit_mask; /* all args quoted */
   (*vec)[1][arg->num] |= set_bit_mask;  /* number of args */
   set_bit_mask <<= 1;



   /* stacks arg style */
   if(arg->firstalt != '\0')
   {
     (*vec)[1][arg->firstalt] |= set_bit_mask;
   }
   set_bit_mask <<= 1;

   for(i = 'a'; i <= 'h' ; i++) /* for the different chars for the stacks */
   {
     (*vec)[1][i] |= set_bit_mask;
   }
   set_bit_mask <<= 1;

   for(i = '0'; i <= '9' ; i++) /* for the different chars for the numbers */
   {
     (*vec)[1][i] |= set_bit_mask;
   }
   (*vec)[1][arg->all] |= set_bit_mask;  /* all args */
   (*vec)[1][arg->allq] |= set_bit_mask; /* all args quoted */
   (*vec)[1][arg->num] |= set_bit_mask;  /* number of args */

}




int add_to_wordlist2(wordlist *list, int macro_name, int macro)
{
  int i, j;
  int empty = -1;
  uint64_t set_bit_mask;
  
  
  int macro_name_len;
  
  /* first entry in list is a start (not checked) with a length of the name in the list */
  macro_name_len = (charstr[macro_name]).data.size;
  
  
  /* first find (new) empty place in list */
  while(empty < 0)
  {
    
    if(list->used_words < list->num_words)
    {
      /* there exists an empty place in this list */
      if(debug)
      {
        printf(" trying to find empty place in list\n");
      }
      i = 0;
      while((i < list->num_words) && (empty < 0))
      {
        if(debug)
        {
          printf(" i = %i, word length = %i\n", i, list->word_length[i]); 
        }
        
        if(list->word_length[i] == 0)
        {
          /* this place is empty */
          empty = i;
          list->used_words++;
        }
        i++;
      }
    }
    else
    {
      if(list->num_words >= 64)
      {
        /* this list is full */
        if(debug)
        {
          printf(" error: list is full, do something about it!\n");
        }
        
        if(list->next == NULL)
        {
          /* create new list */
          list->next = init_wordlist(list->word_size);
          list = list->next;
          empty = 0;
        }
        else
        {
          /* go to next list */
          list = list->next;
        }
        
      }
      else
      {
        /* empty place at end of the words in this list */
        empty = list->num_words;
        list->num_words++;
        list->used_words++;
      }    
      
    }
  }
  
  set_bit_mask = 0b1LL << empty;
  
  /* next entry should have the first character data */
  macro_name++;
  
  /* go through the list up to the end */
  j = 0;
  while((charstr[macro_name]).data.type != charrtype_end)
  {
    /* set the bits in the vectors */
    for(i = (charstr[macro_name]).data.start; i <= (charstr[macro_name]).data.end; i++)
    {
      list->vecs[j][i] |= set_bit_mask;
    }
    
    /* go to next char in vector ? */
    if((charstr[macro_name]).data.size == 1)
    {
      j++;
    }       
    
    macro_name++;
  };
  
  list->word_length[empty] = macro_name_len;
  list->macro[empty] = macro;
  
  if(debug)
  {
    printf("\n add list place: %i length: %i mask: %li macro: %i\n", empty , macro_name_len, set_bit_mask, macro);
  }
  
  return(empty);
}


void delete_from_wordlist(wordlist *list, int entry)
{
  int i,
      j;
  uint64_t set_bit_mask;

      
  list->word_length[entry] = 0;  
  list->used_words--;

  set_bit_mask = ~(0b1LL << entry);
  

  /* go through the list up to the end */
  for(j = 0; j < list->word_size; j++)
  {
    /* set the bits in the vectors */
    for(i = 0; i <= 255; i++)
    {
      list->vecs[j][i] &= set_bit_mask;
    }
  }
  
  if(debug)
  {
    printf("\n deleting list place: %i \n", entry);
  }
  
}

void delete_from_pattern(pattern_data *patlist, int vec_num, int vec_index) 
{
  int i,
      j;
  uint64_t set_bit_mask,
  check_mask;
  pattern_vectors (*vec);    
  
  vec = patlist->vec;
  
  check_mask = patlist->masks[vec_num].masks[vec_index];
  
  i = patlist->masks[vec_num].masks_run_patlen[vec_index];
  
  while(i > 0)
  {
    set_bit_mask = 0ULL;
   
    /* make the clearing mask */
    while((i > 0) && (check_mask != 0ULL)) 
    {
      set_bit_mask |= check_mask;
      check_mask >>= 1;
      i--;
    }  
    
    set_bit_mask = ~set_bit_mask;
    
    /* reset all */
    patlist->masks[vec_num].masks_run_patlen[vec_index] = 0;
    patlist->masks[vec_num].masks[vec_index] = 0ULL;
    
    patlist->masks[vec_num].init &= set_bit_mask;
    patlist->masks[vec_num].mask &= set_bit_mask;
    patlist->masks[vec_num].starmask &= set_bit_mask;
    patlist->masks[vec_num].zeromask &= set_bit_mask;
    
    /* set the bits in the vectors */
    for(j = 0; j < size_index; j++)
    {
      (*vec)[vec_num][j] &= set_bit_mask;
    }
    
    if(check_mask == 0ULL)
    {
      /* need to start at the end of the preceding vectors and masks */
      check_mask = 0b1LL << 63;
      vec_num --;
    }
  }
  
  if(debug)
  {
    printf("\n deleting pattern place vector: %i  index: %i\n", vec_num, vec_index);
  }
  
}



