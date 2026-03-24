/*
 * Copyright 2025, 2026 Marco de Beurs
 * Copyright 2025 Alex de Beurs
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
#include "definesizes.h"
#include "exitcodes.h"
#include "input.h"
#include "output.h"
#include "xmalloc.h"
#include "sds.h"
#include "bitapvec.h"
#include "stack.h"
#include "processor.h"
#include "macros.h"
#include "statistics.h"




/* history buffer
 * 
 * Used to hold the bitap states and input so that
 * a jump back will start the bitap algo correctly.
 * A jump back happens when a macro gets replaced.
 *
 * his_checks: holds the values of the bitap registers.
 * These are also the working registers of the bitap algos.
 *
 * his_inchar: holds the input character
 *
 * his_index: holds the start position in his_checks for
 * the set of registers.
 *
 * his_size_vlm: holds the size of the vlm part in the 
 * set of registers.
 *
 * his_size_pat: holds the size of the pattern part in
 * the set of registers.
 *
 * his_inchar, his_index, his_size_vlm and his_size_pat are
 * arrays whereby the index of the array is the position
 * in the history buffer.
 *
 * The current position is stored in: his.position 
 *
 * set of registers:
 * 1. number of registers for macros (his.incr_macro)
 *     normally 3.
 *       check[0]
 *       check[1]
 *       check[2]
 * 2. number of optional registers for variable length
 *     macros (his.incr_vlm) normally 3 times number of 
 *     vlm vector sets.
 *       check
 *       memory
 *       mask
 * 3. number of optional registers for patterns 
 *     (his.incr_pattern) normally 3 times number of 
 *     pattern vector sets.
 *       check
 *       memory
 *       mask
 *   
 */ 


/* memory for history of bitap */
uint64_t *his_checks;
uint8_t  *his_inchar;
int      *his_index;
short int *his_size_vlm;
short int *his_size_pat;

typedef struct 
{
  int position,     /* current position in the history buffer (chars) */
      local_begin;  /* begin of the buffer */
  int inchar_size;  /* last possible position */
  int checks_size;  /* last possible position */
  int incr_size,    /* current number of checks needed for one char */
      incr_macro,  /* increment for macro bitap */
      incr_vlm,  /* increment for macro bitap */
      incr_checks,  /* increment for macro bitap + vlm */
      incr_pattern; /* increment for patterns */
} history_bitap;

history_bitap his;

typedef struct
{
  uint64_t check,
           mem,
           onetimemask;
} pattern_registers;


#define pattern_check_size (sizeof(pattern_registers) / sizeof(uint64_t)) 


status_pattern stat_pat;

int line_count_flag = 0;

/* trace info */
trace_setting trace;

typedef enum
{
  Normal_macro,
  Variable_length_macro
} type_of_macro;


void process_virtual(uint8_t);



static inline int set_pattern_history(int size)
{
  int prev;
  
  prev = his.incr_pattern;
  
  his.incr_pattern = size;
  his.incr_size = his.incr_checks + size * pattern_check_size;
  
  return(prev);
}

static inline void set_vlm_history(void)
{
 
  
  if(current_status_bitap->patlist != NULL)
  {
    his.incr_vlm = (current_status_bitap->patlist->vec_size / 64 + 1) * pattern_check_size;
    his.incr_checks = his.incr_macro + his.incr_vlm;
  }
  else
  {
    his.incr_vlm = 0;
    his.incr_checks = his.incr_macro;
  }

  his.incr_size = his.incr_checks + his.incr_pattern * pattern_check_size;
  
}

void set_vlm_masks_history(void)
{
  pattern_masks *masks;
  pattern_registers *patcheck;
  int i;
  
  /* set the start masks of the vlm */
  if(current_status_bitap->patlist != NULL)
  {
    
    masks = current_status_bitap->patlist->masks;
    
    patcheck = (pattern_registers *)(his_checks + his_index[his.position] + his.incr_macro);
    
    for(i = 0 ; i < current_status_bitap->patlist->vec_size; i++)
    {
      patcheck[i].onetimemask = masks[i].mask;
      
      if(debug)
      {
        print_bits(masks[i].mask);
      }
    }
  }
}

static inline void clear_step_history(void)
{
  int i,
      start;
  
  start = his_index[his.position];
  
  for(i = start; i < (start + his.incr_size); i++)
  {
    his_checks[i] = 0ULL;
  }

  set_vlm_masks_history();

  if(debug)
  {
    printf(" clearing history: %i, %i\n", his.position, his_index[his.position]);
  }
  
}

void check_history_size(void)
{
  int i,
      start,
      adapt_vlm,
      adapt_pat;
  
  if((his_size_vlm[his.position] < his.incr_vlm) || (his_size_pat[his.position] < his.incr_pattern))
  {
    /* need to adapt the entry size */
    adapt_vlm = his.incr_vlm - his_size_vlm[his.position];
    adapt_pat = his.incr_pattern - his_size_pat[his.position];

    if(debug)
    {
      printf(" adapting history entry: %i, adapt vlm: %i, adapt pat: %i\n", his.position, adapt_vlm, adapt_pat);
    }

    if(his_index[his.position] >= (his.checks_size - his.incr_size))
    {
      /* need more room */
      his_checks = xrealloc(his_checks, sizeof(uint64_t) * (his.checks_size + size_history_checks));
      his.checks_size += size_history_checks;
    }

    /* move pattern */
    start = his_index[his.position] + his.incr_macro + his_size_vlm[his.position];
  
    for(i = start; i < (start + his_size_pat[his.position]); i++)
    {
      his_checks[i + adapt_vlm] = his_checks[i];
    }

    /* reset new pattern */
    start = his_index[his.position] + his.incr_checks + his_size_vlm[his.position];
  
    for(i = start; i < (start + adapt_pat); i++)
    {
      his_checks[i] = 0ULL;
    }

    /* reset new vlm */
    start = his_index[his.position] + his.incr_macro + his_size_vlm[his.position];
  
    for(i = start; i < (start + adapt_vlm); i++)
    {
      his_checks[i] = 0ULL;
    }

    set_vlm_masks_history();
  }
  
}


void init_history_mem(int size_chk)
{
  
  his_checks = xmalloc(sizeof(uint64_t) * size_history_checks);
  his.checks_size = size_history_checks;
  
  his_inchar = xmalloc(sizeof(uint8_t) * size_history_chars);
  his_index = xmalloc(sizeof(int) * size_history_chars);
  his_size_vlm = xmalloc(sizeof(short int) * size_history_chars);
  his_size_pat = xmalloc(sizeof(short int) * size_history_chars);
  his.inchar_size = size_history_chars;
  
  his.position = 0;
  his.local_begin = 0;
  his_index[0] = 0;
  his_inchar[0] = '\n';
  
  
  his.incr_macro = size_chk;
  his.incr_pattern = 0;
  
  set_vlm_history();
  
  clear_step_history(); 
  
  his_size_vlm[0] = his.incr_vlm;
  his_size_pat[0] = his.incr_pattern;
}



uint64_t *step_history(void)
{
  uint64_t *reg;
  uint64_t check;
  int prev_index,
      i;
  
  prev_index = his_index[his.position];
  
  
  /* see if the history buffer can be reduced */
  if(his.position >= size_reduce_history_chars)
  {
    /* this is only for statistics */
    if(his.position > max_position)
    {
      max_position = his.position;
    }
    
    reg = his_checks + his_index[his.position];
    check = 0ULL;
    /* are all checks of the macro bitap zero */
    for(i = 0; i < his.incr_macro; i++)
    {
      check |= *reg;
      reg++;
    }

    /* are all checks of the vlm zero */
    for(i = 0; i < his.incr_vlm; i++)
    {
      check |= *reg;
      reg++;
      check |= *reg;
      reg += 2;
    }

    /* are all checks of the pattern bitap zero */
    for(i = 0; i < his.incr_pattern; i++)
    {
      check |= *reg;
      reg++;
      check |= *reg;
      reg += 2;
    }
    
    /* if all checks are zero then the buffer can be reduced */
    if(check == 0ULL)
    {
      his.position = his.local_begin;
      prev_index = his_index[his.position];

      check_history_size();
    }
  }
  
  if(debug)
  {
    printf(" previous history, char = %c, his pos = %i, index = %i\n", his_inchar[his.position], his.position, his_index[his.position]);
    reg = his_checks + his_index[his.position];
    printf(" history macro checks:\n");
    for(i = 0; i < his.incr_checks; i++)
    {
      print_bits(*reg);
      reg++;
    }
    for(i = 0; i < his.incr_pattern; i++)
    {
      printf(" history pattern checks, mem, mask [%i]:\n",i);
      print_bits(*reg);
      reg++;
      print_bits(*reg);
      reg++;
      print_bits(*reg);
      reg++;
    }
    
  }
  
  
  his.position++;
  
  if(his.position >= his.inchar_size)
  {
    /* need more room */
    his_inchar = xrealloc(his_inchar, sizeof(uint8_t) * (his.inchar_size + size_history_chars));
    his_index = xrealloc(his_index, sizeof(int) * (his.inchar_size + size_history_chars));
    his_size_vlm = xrealloc(his_size_vlm, sizeof(short int) * (his.inchar_size + size_history_chars));
    his_size_pat = xrealloc(his_size_pat, sizeof(short int) * (his.inchar_size + size_history_chars));
    his.inchar_size += size_history_chars;
  }
  
  his_index[his.position] = prev_index + his.incr_size;
    
  if(his_index[his.position] >= (his.checks_size - his.incr_size))
  {
    /* need more room */
    his_checks = xrealloc(his_checks, sizeof(uint64_t) * (his.checks_size + size_history_checks));
    his.checks_size += size_history_checks;
  }
  
  his_size_vlm[his.position] = his.incr_vlm;
  his_size_pat[his.position] = his.incr_pattern;
  
  return( his_checks + his_index[his.position]);
}

void print_history(void)
{
  uint64_t *reg;
  int i;
  
  
  if(debug)
  {
    printf(" CURRENT history, char = %c, his pos = %i, index = %i\n", his_inchar[his.position], his.position, his_index[his.position]);
    reg = his_checks + his_index[his.position];
    printf(" history macro checks:\n");
    for(i = 0; i < his.incr_checks; i++)
    {
      print_bits(*reg);
      reg++;
    }
    for(i = 0; i < his.incr_pattern; i++)
    {
      printf(" history pattern checks, mem, mask [%i]:\n",i);
      print_bits(*reg);
      reg++;
      print_bits(*reg);
      reg++;
      print_bits(*reg);
      reg++;
    }
  }      
  
}


int find_macro(wordlist *list, uint8_t *endmacroname, int length)
{
  int macro = -1;
  int i;
  uint64_t result;
  uint8_t *name;
  
  if(debug)
  {
    printf(" find macroname %.*s \n",length ,endmacroname - length + 1);
  }
  
  do
  {
    name = endmacroname - length + 1;  /* set pointer to first char of the macro word */

    /* the bitap algo */
  
    result = 0xffffffffffffffffULL;
  
    
    for(i=0; i < length; i++)
    {
      result &= list->vecs[i][*name];
      name++;
    }
  
    /* find the word in the list */
    if(result != 0LL)
    {
      i = 0;
      while(!(((result & 0b1ULL) == 0x1ULL) && (list->word_length[i] == length)) && (i < list->num_words))
      {
        result >>= 1;
        i++;
        
      }
    
      if(i < list->num_words)  /* the word is found in the list */
      {
        macro = list->macro[i];
      }
    }
    
    /* possible next wordlist */
    list = list->next;
    
  } while ((macro == -1) && (list != NULL)); /* exit when macro found or searched last list */
  
  
  return(macro);
}


int find_vlm(int vec_size, pattern_masks *masks, pattern_vectors (*vec), uint8_t *endmacroname, int length)
{
  uint64_t prev_check,
           mem[vec_size],
           check[vec_size],
           arg_result;
  uint8_t *name;
  int vecnum;
  int macro = -1;
  int i;
  
  
  name = endmacroname - length + 1;  /* set pointer to first char of the macro word */

  if(debug)
  {
    printf(" find vlm %.*s \n",length , name);
  }

  
  for(vecnum = 0; vecnum < vec_size; vecnum++)
  {
    mem[vecnum] = 0ULL;
    check[vecnum] = 0ULL;
  }
  
  /* first bitap check over complete input */
  for(i=0; i < length; i++)
  {
    for(vecnum = 0; vecnum < vec_size; vecnum++)
    {
      
      /* the shift of the bitap for new step */
      check[vecnum] <<= 1;
      if((vecnum > 0) && (check[vecnum - 1] & (0b1ULL << 63)))
      {
        check[vecnum] |= 0b1ULL;
      }
      
      
      /* the first (main) part bitap */
      /* bitap check with memory for one or more characters */
      check[vecnum] |= (mem[vecnum] << 1);
      if((vecnum > 0) && (mem[vecnum - 1] & (0b1ULL << 63)))
      {
        check[vecnum] |= 0b1ULL;
      }
      
      /* initialise the first character of a search word */
      check[vecnum] |= masks[vecnum].init;
      
      /* bitap check for zero characters */
      if((vecnum > 0) && ((check[vecnum - 1] & masks[vecnum - 1].zeromask) & (0b1ULL << 63)))
      {
        check[vecnum] |= 0b1ULL;
      }
      /* have to repeat to find all zeros */
      do
      {
        prev_check = check[vecnum];
        check[vecnum] |= (check[vecnum] & masks[vecnum].zeromask) << 1;
      } while (prev_check != check[vecnum]);
      
    }
    
    for(vecnum = 0; vecnum < vec_size; vecnum++)
    {
      /* the first (main) part bitap */
      /* (the normal) bitap check for the current character */
      check[vecnum] &= (*vec)[vecnum][*name];
      
      /* second (update) part bitap */
      /* memory checks update for new step */
      mem[vecnum] |=  check[vecnum];
      mem[vecnum] &= (*vec)[vecnum][*name];
      mem[vecnum] &= masks[vecnum].starmask;
      
    }
    
    name++;
  }
  
    
  
  /* find vlm macro */
  vecnum = 0;
  while( (macro < 0) && (vecnum < vec_size) )
  {
    arg_result = check[vecnum] & masks[vecnum].mask;
    
    if(arg_result != 0LL)
    {
      i = 0;
      while(i < masks[vecnum].masks_end)
      {
        if((masks[vecnum].masks[i] & arg_result) != 0LL) 
        {
          /* found the vlm macro */
          macro = masks[vecnum].masks_run[i];
          i = masks[vecnum].masks_end; /* end the loop */
        }
        i++;
      }
    }
    vecnum++;
  }
  
  return(macro);
}



void init_definition(char *def)
{
  int size,
      i,
      name_size,
      def_size;  

  size = strlen(def);
  
  /* find = */
  i = 0;
  while((i < size) && (def[i] != '='))
  {
    i++;
  }
  
  if(i < size)
  {
    /* found an '=' */
    name_size = i;
    def_size = size - name_size - 1;
  }
  else
  {
    name_size = size;
    def_size = 0;
  }
  
  if(name_size == 0)
  {
      fprintf(stderr, "Macro name in options is too short.\n");
      exit(Exit_user);
  }
    
  // printf(" name size: %i, def size %i, def pos: %i\n", name_size, def_size, i+1); 
  
  start_local_stacks();
  push_str(0, "first", 5);  /* first arg is calling name  */
  push_str(0, def, name_size);
  if(def_size > 0)
  {
    push_str(0, &def[i+1], def_size);
  }
  else
  {
    push_str(0, "", 0);
  }
  push_str(0, "", 0);
  push_str(0, "nn00", 4);
  
  define_macro(NULL);
  
  end_local_stacks();
}


void init_process(char *filename, data_buffer *buf)
{
  
  /* open output */
  if(filename[0] == '-' && filename[1] == '\0')
  {
    /* stdout as output */
    buf->file = STDOUT_FILENO;
  }
  else
  {
    /* open file for output */
    buf->file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (buf->file < 0)
    {
      fprintf(stderr, "Error; can not open file for writing: %s\n", filename);
      exit(Exit_io);
    }
  }
  
  output_buffer = buf;
  
  buf->position = 0;
  buf->filename = filename;
  buf->divnum = 0;


  new_vectorset("0",1);
  select_vectorset("0",1);
  
  /* reserve space for lists */
  init_macros();
  init_charstr();
  
  init_asciitohex();
  init_asciitoradix();
  
  init_stacks();
  init_program_list();

  init_div_list();

  /* init history vars */
  init_history_mem(3);
  
  /* set characters for charstr function */
  current_charstr_chars = init_charstr_chars('[', ']', '-', '\\', '@', '+', '*', '?', '~');

  /* set characters for argument placement */
  // current_arg_chars = init_arg_chars('$', '*', '@', '#', '$');
  // add_arg_to_vectors(current_vec, current_arg_chars);

  
  start_local_stacks();
  push_str(0, "Pattern", 7);  /* first arg is calling name  */
  push_str(0, "0", 1);
  push_str(0, "[@00-@39@3b-@ff]~", 17);
  push_str(0, "argpos =0 ifthen abort", 22);
  
  add_pattern(NULL);
  
  end_local_stacks();

  
  start_local_stacks();
  push_str(0, "Pattern", 7);  /* first arg is calling name  */
  push_str(0, "0", 1);
  push_str(0, "[\n;]", 4);
  push_str(0, "end", 3);
  
  add_pattern(NULL);
  
  end_local_stacks();

  start_local_stacks();
  push_str(0, "Pattern", 7);  /* first arg is calling name  */
  push_str(0, "0", 1);
  push_str(0, "[:;]", 4);
  push_str(0, "begin", 5);
  
  add_pattern(NULL);
  
  end_local_stacks();

  start_local_stacks();
  push_str(0, "Pattern", 7);  /* first arg is calling name  */
  push_str(0, "0", 1);
  push_str(0, "\n", 1);
  push_str(0, "stop", 4);
  
  add_pattern(NULL);
  
  end_local_stacks();

  arglist = NULL;
  
  start_local_stacks();
  push_str(0, "first", 5);  /* first arg is calling name  */
  push_str(0, "0_define:", 9);
  push_str(0, "", 0);
  push_str(0, "define", 6);
  push_str(0, "nr01", 4);
  push_str(0, "0", 1);
  
  define_macro(NULL);
  
  end_local_stacks();

}



void close_process(data_buffer **buf)
{
  data_buffer *in;
   
  /* process the at last buffer */ 
  in =  alloc_io_buffer(init_size_processbuf);
  in->file = -1; /* so not real output, but a memory buffer */
  in->position = 0;
  in->divnum = 0;
 
  flush_diversion(0, &in); /* at last buffer is diversion number 0 in the diversion list */

  in->length = in->position;
  in->position = 0;
  
  process_input(&in, buf, Run_macro_yes);

  xfree(in);
  
  /* write possible remaining data */
  flush_all_diversions(buf);
  write_output(buf, 0);
  
  if((*buf)->file != STDOUT_FILENO) /* if stdout do not close */
  {
    /* close file */
    if(close((*buf)->file) < 0)
    {
      fprintf(stderr, "Error closing output file: %s\n", (*buf)->filename);
      exit(Exit_io);
    }
  }
 
}


void set_status_pat(status_pattern *stat_pat)
{
    pattern_masks *masks;
    pattern_registers *patcheck;
    int i;

    masks = arglist->masks;

    patcheck = (pattern_registers *)(his_checks + his_index[his.position] + his.incr_checks);

    if(debug)
    {
      printf(" setting status of pattern, patcheck = %p,  his position = %i \n", patcheck, his.position);
    }
    for(i = 0 ; i < arglist->vec_size; i++)
    {
      patcheck[i].onetimemask = masks[i].mask;

      if(debug)
      {
        print_bits(masks[i].mask);
      }
    }

}

static inline void reset_start_pat(status_pattern *stat_pat)
{
  int i;
 
  for(i = 0; i < number_of_default_stacks; i++)
  {
    stat_pat->stat_arg[i] = no_arg_sampling;
    stat_pat->start[i] = 0;
  }
}

static inline void reset_status_pat(status_pattern *stat_pat)
{

  reset_start_pat(stat_pat);
  
  stat_pat->num_of_args = 0;
  stat_pat->base_of_args = 0;
  stat_pat->overrule = macro_setting_overrule_no;
}


void fill_arguments_pat(macro_def *macro, data_buffer **out)
{
  data_buffer *def;
  pattern_data *backup_arglist;
  arg_text_return ret;
  int new_pat_size,
      prev_local_position,
      prev_pat_size,
      prev_his_position;

  
  /* input buffer for algo */ 
  if(macro->def_len > 0)
  {
    /* definition is from macro definition */
    def = alloc_io_buffer(macro->def_len);
    def->file = -1; /* so not real output, but a memory buffer */
    def->position = 0;
    def->size = macro->def_len;
    def->length = macro->def_len;
    
    memcpy(def->data, macro->def, macro->def_len);
  }
  else
  {
    /* using argument 1 as definition */
    ret = argument_text(0, 1);

    def = alloc_io_buffer(ret.length);
    def->file = -1; /* so not real output, but a memory buffer */
    def->position = 0;
    def->size = ret.length;
    def->length = ret.length;
    
    memcpy(def->data, ret.str_p, ret.length);
  }

  /* first save status and setup new status */
  /* keep the current active status of the pattern */
  // backup_stat_pat = current_status_pattern;
  backup_arglist = arglist;

  arglist = macro->filllist;  /* get the filllist from the macro  */
  
  /* prepare status of pattern */

  // reset_start_pat(&new_stat_pat);

  /* only if filllist exists will arguments be filled, just to be sure */
  if(arglist != NULL)
  {
    new_pat_size = arglist->vec_size;
    prev_pat_size = set_pattern_history(new_pat_size);

    prev_his_position = his.position;
    prev_local_position = his.local_begin;

    step_history();
    clear_step_history();
    his.local_begin = his.position;
    
    set_status_pat(current_status_pattern);

    
    process_input(&def, out, Run_macro_no);

    
    set_pattern_history(prev_pat_size);

    his.position = prev_his_position;
    his.local_begin = prev_local_position;   
  }
  
  arglist = backup_arglist;
  
  xfree(def);

}


void fill_arguments(macro_def *macro, data_buffer **out)
{
  int i, j,
      pos,
      stacknum,
      num_args,
      start_args,
      end_args;
  uint64_t check = 0ll,
           result;
  uint8_t inp,
          inp_digit;
  arg_text_return ret;
  sds num_of_args;
  int num_digits,
      end_digits,
      step_back;

  /* copy the definition to the output
   * and fill in the arguments
   * using a part of the vectors of the bitap algorithm
   */
  for(i = 0; i < macro->def_len; i++)
  {
    /* the current input byte */
    inp = macro->def[i];  
    
    /* bitap algo */
    check |= init_vector[3];          /* vector[3] has the bits for specific the arguments */ 
    check &= (*current_vec)[1][inp];  /* only using the last part of this vector */

    result = mask_vector[3] & check;

    if(result != 0LL)
    {
      /* found one of the argument strings
       * now check which one 
       */

      if((arg_vector[0] & check) != 0ULL)
      {
        /* simple arg */
        stacknum = 0;
        num_args = current_status_pattern->num_of_args;
        start_args = 1 + current_status_pattern->base_of_args;
        end_args = num_args + 1;
        /* at this moment the output is filled one too far, thus: */
        // (*out)->position--;
        step_back = 1;
      }
      else
      {
        /* stacks arg */
        stacknum = macro->def[i-1] - 'a';
        num_args = get_size_stack(stacknum);
        start_args = 0;
        end_args = num_args;
        /* at this moment the output is filled two too far, thus: */
        // (*out)->position -= 2;
        step_back = 2;
      }

      /* return argument */
      if((inp >= '0') && (inp <= '9'))
      {
        pos = inp - '0';
        // printf(" arg number %c %i, stack %i, ", inp, pos, stacknum);
        
        end_digits = 0; /* default to use for check later */
        num_digits = current_status_bitap->num_digits;
        
        /* digits of arg up to num_digits */
        if((num_digits > 1) && (num_digits <= 9))
        {
          j = i + 1;
          end_digits = i + num_digits;
          
          while((j < end_digits) && (j < macro->def_len))
          {
            inp_digit = macro->def[j];
            if((inp_digit >= '0') && (inp_digit <= '9'))
            {
              pos *= 10;
              pos += inp_digit - '0';

              // /* also run bitap algo  */
              // check |= init_vector[3];          /* vector[3] has the bits for specific the arguments */ 
              // check &= (*current_vec)[1][inp_digit];  /* only using the last part of this vector */
              
              i++;
            }
            else
            {
              end_digits = 0;
            }
            j++;
            
          }
        }
        
        /* digits of arg exactly num_digits - 8 */
        if((num_digits >= 10) && (num_digits <= 15))
        {
          j = i + 1;
          end_digits = i + num_digits - 8;
          
          putchar_buffer(inp, out); /* in case of abort need this in output */
          step_back++;
          
          while((j < end_digits) && (j < macro->def_len))
          {
            inp_digit = macro->def[j];
            if((inp_digit >= '0') && (inp_digit <= '9'))
            {
              pos *= 10;
              pos += inp_digit - '0';
              
              /* also run bitap algo in case of abort */
              check |= init_vector[3];          /* vector[3] has the bits for specific the arguments */ 
              check &= (*current_vec)[1][inp_digit];  /* only using the last part of this vector */
              
              /* copy input */
              putchar_buffer(inp_digit, out);
              step_back++;
              
              i++;
            }
            else
            {
              /* not enough digits, abort */
              end_digits = -1;
            }
            j++;
            
          }
          
          if(j != end_digits)
          {
            end_digits = -1;
          }              
          
        }
        
        /* only if the digits collection are ok */
        if(end_digits >= 0)
        {
          ret = argument_text(stacknum, pos);

          (*out)->position -= step_back;
          
          putchars_buffer(ret.str_p, ret.length, out);
        }
      }

      /* return number of arguments */
      if(inp == current_arg_chars->num)
      {
        num_of_args = sdsfromlonglong((long long int) num_args);

        (*out)->position -= step_back;

        putchars_buffer(num_of_args, sdslen(num_of_args), out);

        sdsfree(num_of_args);
      }

      /* return all arguments */
      if(inp == current_arg_chars->all)
      {
        (*out)->position -= step_back;

        for(j = start_args; j < end_args; j++)
        {
          ret = argument_text(stacknum, j);

          putchars_buffer(ret.str_p, ret.length, out);

          if(j < (end_args - 1))
          {
            putchars_buffer(variables[current_status_bitap->quote_var_separator], sdslen(variables[current_status_bitap->quote_var_separator]), out);
          }
        }
      }

      /* return all arguments quoted */
      if(inp == current_arg_chars->allq)
      {
        (*out)->position -= step_back;
        
        for(j = start_args; j < end_args; j++)
        {
          ret = argument_text(stacknum, j);

          putchars_buffer(variables[current_status_bitap->quote_var_start], sdslen(variables[current_status_bitap->quote_var_start]), out);
          putchars_buffer(ret.str_p, ret.length, out);
          putchars_buffer(variables[current_status_bitap->quote_var_end], sdslen(variables[current_status_bitap->quote_var_end]), out);

          if(j < (end_args - 1) )
          {
            putchars_buffer(variables[current_status_bitap->quote_var_separator], sdslen(variables[current_status_bitap->quote_var_separator]), out);
          }

        }
      }


      check = 0LL; /* continue empty */
    }
    else
    {
      /* copy input */
      putchar_buffer(inp, out);

    }

    /* second part bitap */
    check <<= 1;
  }
  
}

void exec_macrocall(int index, data_buffer *arg_out, data_buffer **in, data_buffer **output);

void macro_or_call(macro_def *macro, data_buffer *arg_out, data_buffer **in, data_buffer **out)
{
  status_bitap *oldvecset;
  arg_text_return ret;
  int macro_num,
      stack_shift;

  
  /* execute the normal macro or
   * execute a call to a macro
   */

  if(macro->mcallset == NULL)
  {
    /* normal macro execution */
    /* execute macro function if defined */
    if(macro->builtin > 0)
    {
      if(debug)
      {
        printf(" Executing builtin macro %i \n", macro->builtin);
      }
      
      internal[macro->builtin].intern(out);

      set_vlm_history(); /* macro could have changed macro set or vlm size */ 

    }
    
    
    /* fill definition if defined */
    if((macro->def_len > 0) || (macro->filllist != NULL))
    {
      /* check for file buffer, this can not be used for arguments */
      if((*out)->file >= 0)
      {
        /* the file buffer should be changed to a memory buffer and reset */
        (*out)->file = -1;
        (*out)->start = 0;
        (*out)->position = 0;
      }
      else
      {
        /* the possible previous part of the out buffer (by macros) should not be copied to output */ 
        (*out)->start = (*out)->position;
      }
      
      if(macro->filllist != NULL)
      {
        /* using a pattern to fill the arguments */
        fill_arguments_pat(macro, out);
      }
      else
      {
        /* using default function to fill the arguments */
        fill_arguments(macro, out);
      }    
    }

  }
  else
  {
    /* execute call to macro */
    
    /* switch to new macro set */
    oldvecset = current_status_bitap;
     // fprintf(stderr, "old macro set: %s  %p\n", current_status_bitap->name, current_word15); 
    set_vectorset(macro->mcallset);
    
    // fprintf(stderr, "new macro set: %s  %p\n", current_status_bitap->name, current_word15); 

    /* find macro */
    if(macro->def_len == 0)
    {
      /* using argument 1 as name of macro */
      ret = argument_text(0, 1);
      stack_shift = 1;
      // fprintf(stderr, " macro arg: %*.*s with len: %i\n",ret.length, ret.length, ret.str_p, ret.length);
    }
    else
    {
      /* definition contains the macro name */
      ret.str_p = macro->def;
      ret.length = macro->def_len;
      stack_shift = 0;
      // fprintf(stderr, " macro def: %*.*s\n",ret.length, ret.length, ret.str_p);
    }
    
    if(ret.length > 15)
    {
      macro_num = find_macro(current_word64, ret.str_p + ret.length - 1, ret.length);
      // fprintf(stderr, " find macro in word 64: %*.*s found %i\n",ret.length, ret.length, ret.str_p,macro_num);
    }
    else
    {
      macro_num = find_macro(current_word15, ret.str_p + ret.length - 1, ret.length);
      // fprintf(stderr, " find macro in word 15: %*.*s found %i\n",ret.length, ret.length, ret.str_p,macro_num);
    }
    
    if((macro_num < 0) && (current_status_bitap->patlist != NULL))
    {
      /* try to find vlm */
      macro_num = find_vlm(current_status_bitap->patlist->vec_size, current_status_bitap->patlist->masks, current_status_bitap->patlist->vec, ret.str_p + ret.length - 1, ret.length);
    }

    
    /* shift stack */
    change_arg_stack_start(stack_shift);
    current_status_pattern->num_of_args -= stack_shift;

    /* call macro */
    if(macro_num >= 0)
    {
      exec_macrocall(macro_num, arg_out, in, out);
    }
    else
    {
      print_warning(Exit_user, "Macro %.*s in call to macro not found.\n", ret.length, ret.str_p);
      // exit(Exit_user);
    }
    
    /* shift stack back */
    change_arg_stack_start(-stack_shift);
    current_status_pattern->num_of_args += stack_shift;

    /* switch back to previous macro set */ 
    set_vectorset(oldvecset);
    
  }

  
}

/* Function to execute macro after call to macro.
 * Will be recursively called if further call to macro.
 * 
 * This is similar to a part of exec_macro
 * 
 * A call to macro will not collect arguments.
 * Therefore all code related to argument collection
 * is not present here compared to exec_macro.
 * 
 */
void exec_macrocall(int index, data_buffer *arg_out, data_buffer **in, data_buffer **output)
{
  data_buffer *out;
  macro_def *macro;
  macro_option_recursive recur;
  

  macro = &(macro_list[index]);
  
  if(debug)
  {
    printf("call macro number %i definition = %s in: %p out: %p\n", index, macro->def, arg_out ,*output);
  }
  
  /* buffer for definition or builtin output */
  out =  alloc_io_buffer(init_size_processbuf);
  out->file = -1; /* so not real output, but a memory buffer */
  out->position = 0;
  

  /* execute optional program */
  if(macro->program >= 0)
  {
    if(debug)
    {
      printf(" executing after args program: %i \n", macro->program);
    }
    exec_program(macro->program, 0, current_status_pattern, &arg_out);
  }

 
  /* take diversion to new output */
  out->divnum = arg_out->divnum;

  /* the out buffer is used for output of builtin and later for definition */
  out->start = out->position;

  
  macro_or_call(macro, arg_out, in, &out);


  /* set macro output recurrent or not
   * depending on overrule setting
   */
  switch(current_status_pattern->overrule)
  {
    case macro_setting_overrule_recursive:
      recur = Recursive_yes;
      break;
    case macro_setting_overrule_not_recursive:
      recur = Recursive_no;
      break;
    case macro_setting_overrule_no:
    default:
      recur = macro->recursive;
      break;
  }
  
  /* set the out for input to the next part */
  out->length = out->position;
  out->position = out->start;

  /* This is the place to check for diversion change and act.
   * The previous macro execution could have changed the diversion.
   * Thus the output buffer should be flushed before the new diversion
   * is set.
   */
  if((*output)->divnum != out->divnum)
  {
    flush_output(output);
    if(out->divnum > 0)
    {
      open_diversion(out->divnum, 0);
    }
  }
  (*output)->divnum = out->divnum;


  if(recur == Recursive_yes)
  {
    
      if(debug)
      {
        printf(" running recursive inpos: %i outpos: %i\n",out->position, (*output)->position);
      }
      
      /* start macro processor */
      arglist = NULL;
      set_pattern_history(0);
      step_history();
      clear_step_history();

      process_input(&out, output, Run_macro_yes);
      
    
  }
  else
  {
    /* the input out could be a file input
     * this needs to be handled differently from a memory buffer
     * the output here is always a memory buffer
     * the output will therefore be set to a copy of file out
     */ 
    if(out->file >= 0)
    {
      // fprintf(stderr,"step 3, pos: %i, len: %i\n", out->position, out->length);
      putchars_buffer(&(out->data[out->position]), out->length - out->position, output);
      // fprintf(stderr,"step 4, pos: %i, len: %i\n", (*output)->position, (*output)->length);

      (*output)->start = out->start;
      (*output)->file = out->file;
      (*output)->filename = out->filename;
      (*output)->prev = out->prev;
      (*output)->divnum = out->divnum;

      out->file = -1; /* to not have the current file be set back */
      
      current_input_file_buffer = *output;
  
    }
    else
    {
      /* copy the result to output */
      putchars_buffer(&(out->data[out->position]), out->length - out->position, output);
    }
  }

  /* and set current input file back to previous */
  if((out->file >= 0) && (out->prev != NULL))
  {
    current_input_file_buffer = out->prev;
  }
  /* and free the data buffer of this macro */
  xfree(out);


}


/* function to execute all variants of macros
 * 
 * the data of the macro is:
 * 
 *                   |+++++++++|--------------|++++++++++|xxxxxxxxxxxxx|
 *                   |pre_size | macro_size   |post_size |
 *                   |     macro_len                     |
 *                   |                        |  argument data         |
 *                   |         |   macro definition                    |
 * 
 * on entry to the function the buffer positions are:
 * in->position                                         x    last byte
 * output->position                                     x    last byte
 * 
 * on exit these positions are in case of
 * collection of arguments:
 * in->position                                                       x last byte of arguments
 * 
 * no collection of arguments or abort:
 * in->position                                x first byte of post_size
 * 
 * before copying result to output:
 * output->position             x first byte of macro_size = last byte of pre_size + 1
 * 
 * on exit:  
 * output->position         last byte of pre_size + macro definition  x
 * 
 */

void exec_macro(int index, int macro_len, data_buffer **input, data_buffer **output)
{
  status_pattern *backup_stat_pat,
                 new_stat_pat;
  pattern_data *backup_arglist;
  data_buffer *out,
              *arg_out;
  macro_def *macro;
  int macro_size,
      post_size,
      new_pat_size,
      prev_local_position,
      prev_pat_size,
      prev_his_position,
      max_reserve;
  macro_option_recursive recur;
  
  
  macro = &(macro_list[index]);
  
  if(debug)
  {
    printf(" macro number %i definition = %s in: %p out: %p\n", index, macro->def, *input, *output);
  }
  
  /* buffer for definition or builtin output */
  out =  alloc_io_buffer(init_size_processbuf);
  out->file = -1; /* so not real output, but a memory buffer */
  out->position = 0;
  
  
  /* buffer for argument output */
  arg_out =  alloc_io_buffer(init_size_processbuf);
  arg_out->file = -1; /* so not real output, but a memory buffer */
  arg_out->position = 0;
  arg_out->divnum = (*output)->divnum;
  
  
  /* set the stack for local access */
  start_local_stacks();

  
  if(macro->post_size < 0)
  {
    macro_size = macro_len - macro->pre_size;

    post_size = macro_len - macro->pre_size;
  }
  else
  {
    /* copy the name of the macro to first position on stack */ 
    macro_size = macro_len - macro->post_size - macro->pre_size;
  
    post_size = macro->post_size;
  }

  if(macro_size < 0)
  {
    print_warning(Exit_user, "macro: %.*s size has become less than zero, pre size: %i, post size: %i\n", &((*output)->data[(*output)->position - macro_len + 1]), macro->pre_size, post_size); 
    exit(Exit_user);
  }
  
  push_text(0, output, (*output)->position - macro_len + macro->pre_size + 1, macro_size);

  
  macro_depth++;

  if(macro_depth >= max_recursion_depth)
  {
    fprintf(stderr," Error, maximum recursion depth reached: %i\n", macro_depth);
    exit(Exit_max_recursion);
  }

  if(trace == Trace_on)
  {
    trace_line(macro_depth);
    output_trace(&((*output)->data[(*output)->position - macro_len + macro->pre_size + 1]), macro_size);
    output_trace(" == ",4);
  }
  
  /* collect arguments */
  
  /* first save status and setup new status */
  backup_stat_pat = current_status_pattern;
  backup_arglist = arglist;
  
  arglist = macro->arglist;  /* get the arglist from the macro  */
  
  /* prepare status of pattern */
  current_status_pattern = &new_stat_pat;
  
  reset_status_pat(&new_stat_pat);
  
  if(arglist != NULL)
  {
    new_pat_size = arglist->vec_size;
  }
  else
  {
    new_pat_size = 0;
  }
  prev_his_position = his.position - (macro_len - macro->pre_size);
  
  prev_local_position = his.local_begin;
  
  prev_pat_size = set_pattern_history(new_pat_size);
  step_history();
  clear_step_history();
  his.local_begin = his.position;
  
  if(debug)
  {
    printf(" rewinding history to: %i, with prev pattern size: %i, and new pattern size: %i\n", prev_his_position, prev_pat_size, new_pat_size);
  }
  
  /* only if arglist exists will arguments be collected */ 
  if(arglist != NULL)
  {
    /* reserve space for registers of algo */
    set_status_pat(&new_stat_pat);
    
    print_history();
    
    /* back up pos in input to real end of macro */
    (*input)->position -= post_size - 1; /* next char in input */
    
    if((*input)->position >= (*input)->length)
    {
      if((*input)->file >= 0)
      {
        /* input is file */
        /* read the next part of the input file */
        max_reserve = max_size_macro;
        if(vlm_reserve > max_reserve)
        {
          max_reserve = vlm_reserve;
        }
        read_input(input, max_reserve);
      }
      /* else end of input memory buffer, this should not happen */
    }
    
    /* for proper line counting */
    line_count_flag = -post_size;
    
    /* trick to skip macro expansion in the first number of chars */
    if(macro->arg_type == Run_macro_yes)
    {
      process_input(input, &arg_out, macro->arg_type + post_size);
    }
    else
    {
      process_input(input, &arg_out, macro->arg_type);
    }
    
    // in = *input;
    
    (*input)->position--; /* position 1 back because it will be incremented in process_input */
    
  }
  else
  {
    if(debug)
    {
      printf(" no argument parsing \n");
    }
    /* need to put last part of macro back in input */
    (*input)->position -= post_size; 
  }
  
  
  /* execute optional program after argument collection */
  if(macro->program >= 0)
  {
    if(debug)
    {
      printf(" executing after args program: %i \n", macro->program);
    }
    exec_program(macro->program, 0, current_status_pattern, &arg_out);
  }

  if(trace == Trace_on)
  {
    output_trace(arg_out->data, arg_out->position);
    output_trace(" => ",4);
  }
  
  
  /* maybe diversion changed, take change to new output */
  out->divnum = arg_out->divnum;

  /* the out buffer is used for output of builtin and later for definition */
  out->start = out->position;


  
  /* execute the macro function */
  macro_or_call(macro, arg_out, input, &out);
  

  /* set macro output recurrent or not
   * depending on overrule setting
   */
  switch(current_status_pattern->overrule)
  {
    case macro_setting_overrule_recursive:
      recur = Recursive_yes;
      break;
    case macro_setting_overrule_not_recursive:
      recur = Recursive_no;
      break;
    case macro_setting_overrule_no:
    default:
      recur = macro->recursive;
      break;
  }
  
  
  /* back to previous status */
  current_status_pattern = backup_stat_pat;
  arglist = backup_arglist;
  
  /* free the local stack, not using the stack anymore */
  end_local_stacks();
  
  /* arg buffer not needed anymore */
  xfree(arg_out);  
  
  /* set the history pointer back to the position before the macro call */
  his.position = prev_his_position;
  
  his.local_begin = prev_local_position;
  
  set_pattern_history(prev_pat_size);
  check_history_size();
  
  
  if(debug)
  {
    printf(" resetting history to: %i with pattern size = %i\n", his.position, prev_pat_size);
  }
  
  /* finally copy the result to the output 
   * or if recursive run through the macro processor
   * after correcting input and output
   */
  
  /* set the out for input to the next part */
  out->length = out->position;
  out->position = out->start;
  

  if(trace == Trace_on)
  {
    output_trace(&(out->data[out->start]), out->length - out->position);
    output_trace("\n",1);
  }

  /* Go back predetermined position in output.
   *
   * At the same time go back for length of macro.
   */
  (*output)->position -= macro_len - macro->pre_size - 1;

  /* This is the place to check for diversion change and act.
   * The previous macro execution could have changed the diversion.
   * Thus the output buffer should be flushed before the new diversion
   * is set.
   */
  if((*output)->divnum != out->divnum)
  {
    flush_output(output);
    if(out->divnum > 0)
    {
      open_diversion(out->divnum, 0);
    }
  }
  (*output)->divnum = out->divnum;

    
  if(recur == Recursive_yes)
  {
    
      if(debug)
      {
        printf(" running recursive inpos: %i outpos: %i\n",out->position, (*output)->position);
      }
      
      /* start macro processor */
      process_input(&out, output, Run_macro_yes);
    
  }
  else
  {
    /* the input out could be a file input
     * this needs to be handled differently from a memory buffer
     */ 
    if(out->file >= 0)
    {
      putchars_buffer(&(out->data[out->position]), out->length - out->position, output);
 
      while( read_input(&out, 0) > 0)
      {
        putchars_buffer(out->data, out->length, output);
      }
  
    }
    else
    {
      /* copy the result to output */
      putchars_buffer(&(out->data[out->position]), out->length - out->position, output);
    }
  }

  if(macro->virtual_char != 255)
  {
    process_virtual(macro->virtual_char);
  }
  
  /* need to reduce the position to the last written position; will be incremented in process_input */
  (*output)->position--;

  
  /* and set current input file back to previous */
  if(out->file >= 0 )
  {
    sdsfree(out->filename);
    close_input(out);
    
    if(out->prev != NULL)
    {
      current_input_file_buffer = out->prev;
    }
  }
  /* and free the data buffer of this macro */
  xfree(out);

  macro_depth--;

  if(debug)
  {
    printf(" end of macro inpos: %i outpos: %i\n",out->position, (*output)->position);
  }

}


uint64_t run_pattern(int vec_size, pattern_masks *masks, pattern_vectors (*vec), pattern_registers *prev_patcheck, pattern_registers *patcheck, uint8_t inp, uint8_t prev_inp)
{
  uint64_t prev_check,
           pat_active = 0ULL; /* check to see if pattern is still active, like in history */ 
  int vecnum;
  
  for(vecnum = 0; vecnum < vec_size; vecnum++)
  {
    /* second (update) part bitap */
    /* memory checks update for new step */
    patcheck[vecnum].mem = prev_patcheck[vecnum].mem | prev_patcheck[vecnum].check;
    patcheck[vecnum].mem &= (*vec)[vecnum][prev_inp];
    patcheck[vecnum].mem &= masks[vecnum].starmask;
    
    pat_active |= patcheck[vecnum].mem;
    
    /* the shift of the bitap for new step */
    patcheck[vecnum].check = prev_patcheck[vecnum].check << 1;
    if((vecnum > 0) && (prev_patcheck[vecnum - 1].check & (0b1ULL << 63)))
    {
      patcheck[vecnum].check |= 0b1ULL;
    }

    
    /* the first (main) part bitap */
    /* bitap check with memory for one or more characters */
    patcheck[vecnum].check |= (patcheck[vecnum].mem << 1);
    if((vecnum > 0) && (patcheck[vecnum - 1].mem & (0b1ULL << 63)))
    {
      patcheck[vecnum].check |= 0b1ULL;
    }

    /* initialise the first character of a search word */
    patcheck[vecnum].check |= masks[vecnum].init;

    /* bitap check for zero characters */
    if((vecnum > 0) && ((patcheck[vecnum - 1].check & masks[vecnum - 1].zeromask) & (0b1ULL << 63)))
    {
      patcheck[vecnum].check |= 0b1ULL;
    }
    /* have to repeat to find all zeros */
    do
    {
      prev_check = patcheck[vecnum].check;
      patcheck[vecnum].check |= (patcheck[vecnum].check & masks[vecnum].zeromask) << 1;
    } while (prev_check != patcheck[vecnum].check);

  }

  /* the first (main) part bitap */
  for(vecnum = 0; vecnum < vec_size; vecnum++)
  {
    /* (the normal) bitap check for the current character */
    patcheck[vecnum].check &= (*vec)[vecnum][inp];

    pat_active |= patcheck[vecnum].check;
    
    /* copy the masks */
    patcheck[vecnum].onetimemask = prev_patcheck[vecnum].onetimemask;
  }
  
  
  return(pat_active);
}



int get_vlm_length(int vecnum, int entry)
{
  uint64_t mask,
           zeromask,
           check,
           memory,
           prev_mask = 0ULL;
  int len = 0,
      mask_len,
      his_position;
  
  mask = current_status_bitap->patlist->masks[vecnum].masks[entry];
  mask_len = current_status_bitap->patlist->masks[vecnum].masks_run_patlen[entry];
  zeromask = current_status_bitap->patlist->masks[vecnum].zeromask;

  his_position = his.position;
  
  while(mask_len > 0)
  {
    check = his_checks[his_index[his_position] + his.incr_macro + his.incr_vlm * vecnum];
    memory = his_checks[his_index[his_position] + his.incr_macro + his.incr_vlm * vecnum + 1];

    if(mask == 0ULL)
    {
      vecnum--;
      if(vecnum >= 0)
      {
        zeromask = current_status_bitap->patlist->masks[vecnum].zeromask;
      }
      mask = 1ULL << 63;
    }

    prev_mask = mask;
    
    if(mask & check)
    {
      /* check bit set */
      mask >>= 1;
      his_position--;
      mask_len--;
    }
    else
    {
      if(mask & memory)
      {
        /* memory bit set */
        his_position--;
      }
      else
      {
        if(mask & zeromask)
        {
          /* zero bit mask set */
          mask >>= 1;
          mask_len -= 1;
        }
        else
        {
          fprintf(stderr, "error in vlm length\n");
          break;
        }
      }
    }
  }

  /* if the first macro character can be more than one check previous positions */ 
  if(prev_mask & memory)
  {
    /* find the longest fit */
    do
    {
      check = his_checks[his_index[his_position] + his.incr_macro + his.incr_vlm * vecnum];
      his_position--;
    } while( prev_mask & check);
    
    his_position++;
  }
  
  len = his.position - his_position;
  return(len);
}

static inline void run_vlm_check(int *len, int *macro_num,  pattern_masks *masks, pattern_registers *patcheck, type_of_macro *macro_type)
{
  uint64_t arg_result;
  int j,
  vecnum;
  
  
  /* find vlm macro */
  vecnum = 0;
  while( (*macro_num < 0) && (vecnum < current_status_bitap->patlist->vec_size) )
  {
    arg_result = patcheck[vecnum].check & patcheck[vecnum].onetimemask;
    
    if(arg_result != 0LL)
    {
      j = 0;
      while(j < masks[vecnum].masks_end)
      {
        if((masks[vecnum].masks[j] & arg_result) != 0LL) 
        {
          /* found the vlm macro */
          *macro_num = masks[vecnum].masks_run[j];
          *macro_type = Variable_length_macro;
          // *len = masks[vecnum].masks_run_patlen[j];
          *len = get_vlm_length(vecnum, j);
          j = masks[vecnum].masks_end; /* end the loop */

          if(debug)
          {
            printf("len: %i  macronum: %i", *len, *macro_num);
          }
        }
        j++;
      }
    }
    vecnum++;
  }
  
  
}  

static inline void run_pat_check(int macro_num, run_macro *runmacro,  int *arg_level,  pattern_masks *masks, pattern_registers *patcheck, data_buffer **buffer, data_buffer **output, arg_run *stop)
{
  uint64_t arg_result;
  int j,
  vecnum;
  
  if(macro_num >= 0)
  {
    *arg_level = 1; /* set argument level */
  }
  
  
  /* check to see if the patterns are found after the one time check */
  for(vecnum = 0; vecnum < arglist->vec_size; vecnum++)
  {
    arg_result = patcheck[vecnum].check & patcheck[vecnum].onetimemask;
    
    if(arg_result != 0LL)
    {
      j = 0;
      while((j < masks[vecnum].masks_end) && (stop->status == arg_continu))
      {
        if(((masks[vecnum].masks[j] & arg_result) != 0LL) && (masks[vecnum].masks_run_level[j] >= *arg_level) && (stop->status == arg_continu))
        {
          /* found the trigger */
          if(masks[vecnum].masks_run_patlen[j] < 0)
          {
            /* the one time trigger pattern */
            if(masks[vecnum].masks_run[j] >= 0)
            {
              *stop = exec_program(masks[vecnum].masks_run[j], -(masks[vecnum].masks_run_patlen[j]), current_status_pattern, output);
            }
            patcheck[vecnum].onetimemask &= ~(masks[vecnum].masks[j]);
          }
          else
          {
            /* all other patterns */
            if(masks[vecnum].masks_run[j] >= 0)
            {
              *stop = exec_program(masks[vecnum].masks_run[j], masks[vecnum].masks_run_patlen[j], current_status_pattern, output);
            }
          }

          if(stop->status == arg_no_macros)
          {
            /* stop the execution of macros.
             * is set by instruction in pattern program.
             */
            *runmacro = Run_macro_no;
            /* continu with pattern */
            stop->status = arg_continu;
          }

          if(stop->status == arg_abort)
          {
            /* no argument collection, position input should be placed back
             * output is a buffer (during argument collection)
             * thus output->position can be used to determine number of bytes
             */
            (*buffer)->position -= (*output)->position + 1;
            his.position -= (*output)->position;
          }
          
        }
        j++;

      }
    }
  }
  
}  

static inline void run_update_positions(data_buffer **buffer, data_buffer **output)
{
  int max_reserve;
  
    
  (*output)->position++;
  
  if((*output)->position >= (*output)->size)
  {
    if((*output)->file >= 0)
    {
      /* output to real file or stdout */
      max_reserve = max_size_macro;
      if(vlm_reserve > max_reserve)
      {
        max_reserve = vlm_reserve;
      }
      write_output(output, max_reserve);
    }
    else
    {
      /* output is a memory buffer that needs to grow */
      *output = incr_buffer(*output, add_size_processbuf);
    }
  }
  
  (*buffer)->position++;
  if((*buffer)->position >= (*buffer)->length)
  {
    if((*buffer)->file >= 0)
    {
      /* input is file */
      /* read the next part of the input file */
      max_reserve = max_size_macro;
      if(vlm_reserve > max_reserve)
      {
        max_reserve = vlm_reserve;
      }
      read_input(buffer, max_reserve);
      
      if(debug)
      {
        printf("\n\n ---------- \n size: %i  pos: %i \n -------------\n", (*buffer)->length, (*buffer)->position);
      }
    }
    /* else end of input memory buffer, this will end the while loop */
  }
  
}



static inline void run_macros(run_macro *runmacro, int macro_num, type_of_macro macro_type, int *arg_level, int len, data_buffer **input, data_buffer **output)
{
  if((macro_num >= 0) && (*runmacro == Run_macro_yes))
  {
    exec_macro(macro_num, len, input, output);
    
    *arg_level = 0; /* reset argument level */
    
    /* counting macros just for statistics */
    if(macro_type == Normal_macro)
    {
      statistics[len]++;
    }
    else
    {
      stat_vml++;
      if(len > stat_vml_max_len)
      {
        stat_vml_max_len = len;
      }
    }
  }
  
  /* trick to skip running macro for a first number of times */
  if(*runmacro > Run_macro_yes)
  {
    (*runmacro)--;
  }
}


static inline void run_count_lines(uint8_t inp, data_buffer **buffer)
{
  /* count lines */
  if((inp == '\n') && ((*buffer)->file >= 0) && (line_count_flag >= 0))
  {
    line_counter++;
    local_line_counter++;
    
    if(debug)
    {
      printf("\n\n ---------- \n incr LINE: %i \n -------------\n", local_line_counter);
    }
  }
  line_count_flag++;
}



void run_bitap_macro(uint8_t inp, uint64_t *prev_check, uint64_t *current_check)
{
    
  /* second part of bitap 
   * is done just before the first part instead of at the end 
   * since this is a loop, the position does not really matter */
  current_check[0] = prev_check[0] << 1;
  current_check[1] = prev_check[1] << 1;
  current_check[2] = prev_check[2] << 1;
  
  /* bitap algo */
  current_check[0] |= init_vector[0];
  current_check[0] &= (*current_vec)[0][inp];
  
  current_check[1] |= init_vector[1];
  current_check[1] &= (*current_vec)[1][inp];
  
  current_check[2] |= init_vector[2];
  current_check[2] &= (*current_vec)[2][inp];
  
  if(debug)
  {
    printf(" char = %c, checks now:\n", inp);
    print_bits(current_check[0]);
    print_bits(current_check[1]);
    print_bits(current_check[2]);
  }
    
}


static inline void run_macro_check(int *len, int *macro_num, uint64_t *current_check, type_of_macro *macro_type)
{
  uint64_t result[3];
  
  /* check to see if something is found */
  result[2] = mask_vector[2] & current_check[2];
  
  if(result[2] != 0ULL)
  {
    /* check which size is found, starting with the largest first */
    *len = 64;
    while((*len > 15) && (*macro_num < 0))
    {
      if((result[2] & 0b1000000000000000000000000000000000000000000000000000000000000000LL) != 0ULL)
      {
        /* check if macro name is really in the list */
        *macro_num = find_macro(current_word64, &(his_inchar[his.position]), *len);

        if(macro_num > 0)
        {
          *macro_type = Normal_macro;
        }
      }
      result[2] <<= 1;
      (*len)--;
    }
  }
  
  /* check the shorter results only if no macro was found before */
  if(*macro_num < 0)
  {
    
    /* check to see if something is found */
    result[0] = mask_vector[0] & current_check[0];
    result[1] = mask_vector[1] & current_check[1];
    
    if((result[0] != 0ULL || result[1]) != 0ULL)
    {
      if(debug)
      {
        printf("checked result _%c_, his pos: %i\n", his_inchar[his.position], his.position);
        print_bits(result[0]);
        print_bits(result[1]);
      }
      
      /* check which size is found, starting with the largest first */
      *len = 15;
      while((*len > 0) && (*macro_num < 0))
      {
        if((result[l_to_vec[*len]] & mask_length[*len]) != 0ULL)
        {
          /* check if macro name is really in the list */
          *macro_num = find_macro(current_word15, &(his_inchar[his.position]), *len);
  
          if(macro_num > 0)
          {
            *macro_type = Normal_macro;
          }
        }   
        
        
        if(debug)
        {
          printf("len: %i  macronum: %i", *len, *macro_num);
        }
        
        (*len)--;
      }
    }
  }
  
  (*len)++; /* compensate the 1 too many decrement */
}


void process_input(data_buffer **buffer, data_buffer **output, run_macro runmacro)
{
  uint64_t *prev_check,
           *current_check,
           pat_active;
  pattern_registers *prev_patcheck,
                    *patcheck,
                    *vlmcheck,
                    *prev_vlmcheck;
  int len = 0,
      arg_level;
  int macro_num;
  arg_run stop;
  type_of_macro macro_type = Normal_macro;
  uint8_t inp,
          prev_inp;
  
  
  /* stop is used to end the argument collection and thereby also this function call */
  stop.status = arg_continu;

  arg_level = 0;

  
  if(debug)
  {
    current_check = his_checks + his_index[his.position];
    printf(" in: %p, pos: %i, size: %i, length %i \n", *buffer, (*buffer)->position, (*buffer)->size, (*buffer)->length);
    printf(" out: %p, pos: %i, size: %i, length %i \n", (*output), (*output)->position, (*output)->size, (*output)->length);
    printf(" check: %p, 0: %lu, 1: %lu, 2: %lu \n", current_check, current_check[0], current_check[1], current_check[2]);
    printf(" arglist: %p, cur_stat: %p \n", arglist, current_status_pattern);
    printf("\n his position: %i \n\n", his.position);
  }
  

  
  while(((*buffer)->position < (*buffer)->length) && (stop.status == arg_continu))
  {
    /* reset macro number */
    macro_num = -1;
    
    /* set the checks in history array correct: macros can have changed the position 
     * the step_history can also reduce the position 
     */
    current_check = step_history();
    prev_check = his_checks + his_index[his.position - 1];;
    prev_inp = his_inchar[his.position - 1];
    
    
    /* the current input byte */
    inp = (*buffer)->data[(*buffer)->position];
    
    /* copy input to output */
    (*output)->data[(*output)->position] = inp;
    
    /* copy input to history buffer: necessary for complete macro name in call to macros */
    his_inchar[his.position] = inp;
    
    /* bitap algo for finding macros */
    if(runmacro >= Run_macro_yes)
    {
      run_bitap_macro(inp, prev_check, current_check);   
      
      if(runmacro == Run_macro_yes)
      {
        run_macro_check(&len, &macro_num, current_check, &macro_type);
      }
    
      /* if vlm exists then also do vlm checks */ 
      if(current_status_bitap->patlist != NULL)
      {
        vlmcheck = (pattern_registers *)(current_check + his.incr_macro);
        prev_vlmcheck = (pattern_registers *)(prev_check + his.incr_macro);

        if(debug)
        {
          printf("running vlm!\n");
        }

        pat_active = run_pattern(current_status_bitap->patlist->vec_size, current_status_bitap->patlist->masks, current_status_bitap->patlist->vec, prev_vlmcheck, vlmcheck, inp, prev_inp);
      
        /* update possible vlm length */
        if(pat_active)
        {
          vlm_reserve++;
        }
        else
        {
          vlm_reserve = 0;
        }
        
        if(runmacro == Run_macro_yes)
        {
          if(debug)
          {
            printf("and running vlm check!\n");
          }
          run_vlm_check(&len, &macro_num, current_status_bitap->patlist->masks, vlmcheck, &macro_type);
        }
      }
 
    }

    /* bitap algo for argument collection */
    if(arglist != NULL)
    {
      patcheck = (pattern_registers *)(current_check + his.incr_checks);
      prev_patcheck = (pattern_registers *)(prev_check + his.incr_checks);

      run_pattern(arglist->vec_size, arglist->masks, arglist->vec, prev_patcheck, patcheck, inp, prev_inp);
      run_pat_check(macro_num, &runmacro, &arg_level, arglist->masks, patcheck, buffer, output, &stop);

    }
    
    if(debug)
    {
      print_history();
    }
    /* counting lines */
    run_count_lines(inp, buffer);
    
    /* run macro if applicable */
    run_macros(&runmacro, macro_num, macro_type, &arg_level, len, buffer, output);
    
    /* update positions */
    run_update_positions(buffer, output);
    
    /* trick to skip running macro for a first number of times */
    if(runmacro > Run_macro_yes)
    {
      runmacro--;
    }
    
  }
  
  
  if(debug)
  {
    printf("leaving processor function posinp: %i posout: %i fileout: %i\n", (*buffer)->position, (*output)->position, (*output)->file);
  }
}



void process_virtual(uint8_t inp)
{
  uint64_t *prev_check,
           *current_check;
  pattern_registers *prev_patcheck,
                    *patcheck,
                    *vlmcheck,
                    *prev_vlmcheck;
  uint8_t prev_inp;


  current_check = step_history();
  prev_check = his_checks + his_index[his.position - 1];;
  prev_inp = his_inchar[his.position - 1];
   
  patcheck = (pattern_registers *)(current_check + his.incr_checks);
  prev_patcheck = (pattern_registers *)(prev_check + his.incr_checks);
    
  
  his_inchar[his.position] = inp;
  
  if(debug)
  {
    printf("virtual char\n");
  }
 
  /* bitap algo for macros */
  run_bitap_macro(inp, prev_check, current_check);   

  if(current_status_bitap->patlist != NULL)
  {
    vlmcheck = (pattern_registers *)(current_check + his.incr_macro);
    prev_vlmcheck = (pattern_registers *)(prev_check + his.incr_macro);
    
    
    run_pattern(current_status_bitap->patlist->vec_size, current_status_bitap->patlist->masks, current_status_bitap->patlist->vec, prev_vlmcheck, vlmcheck, inp, prev_inp);    
  }
  
  /* bitap algo for argument collection */
  
  if(arglist != NULL)
  {
    run_pattern(arglist->vec_size, arglist->masks, arglist->vec, prev_patcheck, patcheck, inp, prev_inp);
  }  

  
}
