/*
 * Copyright 2025 Marco de Beurs
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


/* memory for history of bitap */
uint64_t *his_checks;
uint8_t  *his_inchar;
int      *his_index;

typedef struct 
{
  int position,     /* current position in the history buffer (chars) */
      local_begin;  /* begin of the buffer */
  int inchar_size;  /* last possible position */
  int checks_size;  /* last possible position */
  int incr_size,    /* current number of checks needed for one char */
      incr_checks,  /* increment for macro bitap */
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

trace_setting trace;
int macro_depth = 0;

void process_virtual(uint8_t);


static inline int set_pattern_history(int size)
{
  int prev;
  
  prev = his.incr_pattern;
  
  his.incr_pattern = size;
  his.incr_size = his.incr_checks + size * pattern_check_size;
  
  return(prev);
}

void init_history_mem(int size_chk)
{
   int i;
   
   his_checks = xmalloc(sizeof(uint64_t) * size_history_checks);
   his.checks_size = size_history_checks;

   his_inchar = xmalloc(sizeof(uint8_t) * size_history_chars);
   his_index = xmalloc(sizeof(int) * size_history_chars);
   his.inchar_size = size_history_chars;
   
   his.position = 0;
   his.local_begin = 0;
   his_index[0] = 0;
   his_inchar[0] = '\n';
   
   for(i = 0; i < size_chk; i++)
   {
     his_checks[i] = 0ULL;
   }
   his.incr_checks = size_chk;
   set_pattern_history(0);
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

  if(debug)
  {
    printf(" clearing history: %i, %i\n", his.position, his_index[his.position]);
  }
  
}

static inline uint64_t *step_history(void)
{
  uint64_t *reg;
  uint64_t check;
  int prev_index,
      i;
  
  prev_index = his_index[his.position];
  
  
  /* see if the history buffer can be reduced */
  if(his.position >= size_reduce_history_chars)
  {
    reg = his_checks + his_index[his.position];
    check = 0ULL;
    /* are all checks of the macro bitap zero */
    for(i = 0; i < his.incr_checks; i++)
    {
      check |= *reg;
      reg++;
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
    his.inchar_size += size_history_chars;
  }
  
  his_index[his.position] = prev_index + his.incr_size;
  
  if(his_index[his.position] >= (his.checks_size - his.incr_size))
  {
    /* need more room */
    his_checks = xrealloc(his_checks, sizeof(uint64_t) * (his.checks_size + size_history_checks));
    his.checks_size += size_history_checks;
  }
  
  
  
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
 
  in = flush_diversion(0, in); /* at last buffer is diversion number 0 in the diversion list */

  in->length = in->position;
  in->position = 0;
  
  process_input(in, buf, Run_macro_yes);

  xfree(in);
  
  /* write possible remaining data */
  *buf = flush_all_diversions(*buf);
  write_output(*buf, 0);
  
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


static inline void set_status_pat(status_pattern *stat_pat)
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
  // status_pattern *backup_stat_pat,
                 // new_stat_pat;
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

    
    process_input(def, out, Run_macro_no);

    
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
  uint8_t inp;
  arg_text_return ret;
  sds num_of_args;

  /* copy the definition to the output
   * and fill in the arguments
   * using a part of the vectors of the bitap algoritm
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

      /* simple arg */
      if((arg_vector[0] & check) != 0ULL)
      {
        stacknum = 0;
        num_args = current_status_pattern->num_of_args;
        start_args = 1 + current_status_pattern->base_of_args;
        end_args = num_args + 1;
        /* at this moment the output is filled one too far, thus: */
        (*out)->position--;
      }
      /* stacks arg */
      // if((arg_vector[1] & check) != 0ULL)
      else
      {
        stacknum = macro->def[i-1] - 'a';
        num_args = get_size_stack(stacknum);
        start_args = 0;
        end_args = num_args;
        /* at this moment the output is filled two too far, thus: */
        (*out)->position -= 2;
      }

      /* return argument */
      if((inp >= '0') && (inp <= '9'))
      {
        pos = inp - '0';
        // printf(" arg number %c %i, stack %i, ", inp, pos, stacknum);
        ret = argument_text(stacknum, pos);

        // printf(" return: %i %p \n", ret.length, ret.str_p);

        *out = putchars_buffer(ret.str_p, ret.length, *out);
      }

      /* return number of arguments */
      if(inp == current_arg_chars->num)
      {
        num_of_args = sdsfromlonglong((long long int) num_args);

        *out = putchars_buffer(num_of_args, sdslen(num_of_args), *out);

        sdsfree(num_of_args);
      }

      /* return all arguments */
      if(inp == current_arg_chars->all)
      {
        for(j = start_args; j < end_args; j++)
        {
          ret = argument_text(stacknum, j);

          *out = putchars_buffer(ret.str_p, ret.length, *out);

          if(j < (end_args - 1))
          {
            *out = putchars_buffer(variables[current_status_bitap->quote_var_separator], sdslen(variables[current_status_bitap->quote_var_separator]), *out);
          }
        }
      }

      /* return all arguments quoted */
      if(inp == current_arg_chars->allq)
      {
        for(j = start_args; j < end_args; j++)
        {
          ret = argument_text(stacknum, j);

          *out = putchars_buffer(variables[current_status_bitap->quote_var_start], sdslen(variables[current_status_bitap->quote_var_start]), *out);
          *out = putchars_buffer(ret.str_p, ret.length, *out);
          *out = putchars_buffer(variables[current_status_bitap->quote_var_end], sdslen(variables[current_status_bitap->quote_var_end]), *out);

          if(j < (end_args - 1) )
          {
            *out = putchars_buffer(variables[current_status_bitap->quote_var_separator], sdslen(variables[current_status_bitap->quote_var_separator]), *out);
          }

        }
      }


      check = 0LL; /* continue empty */
    }
    else
    {
      /* copy input */
      *out = putchar_buffer(inp, *out);

    }

    /* second part bitap */
    check <<= 1;
  }
  
}

void exec_macrocall(int index, data_buffer *arg_out, data_buffer *in, data_buffer **output);

static inline void macro_or_call(macro_def *macro, data_buffer *arg_out, data_buffer *in, data_buffer **out)
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
    }
    
    
    /* fill definition if defined */
    if((macro->def_len > 0) || (macro->filllist != NULL))
    {
      /* the possible previous part of the out buffer (by macros) should not be copied to output */ 
      (*out)->start = (*out)->position;
      
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
      fprintf(stderr, "Error line: %i in file: %s line: %i; Macro %.*s in call to macro not found.\n", line_counter, current_input_file_buffer->filename, local_line_counter,  ret.length, ret.str_p);
      exit(Exit_user);
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
void exec_macrocall(int index, data_buffer *arg_out, data_buffer *in, data_buffer **output)
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
    flush_output(*output);
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

      process_input(out, output, Run_macro_yes);
      
    
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
      *output = putchars_buffer(&(out->data[out->position]), out->length - out->position, *output);
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
      *output = putchars_buffer(&(out->data[out->position]), out->length - out->position, *output);
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

void exec_macro(int index, int macro_len, data_buffer *input, data_buffer **output)
{
  status_pattern *backup_stat_pat,
                 new_stat_pat;
  pattern_data *backup_arglist;
  data_buffer *out,
              *arg_out;
  macro_def *macro;
  int macro_size,
      new_pat_size,
      prev_local_position,
      prev_pat_size,
      prev_his_position;
  macro_option_recursive recur;
  
  
  macro = &(macro_list[index]);
  
  if(debug)
  {
    printf(" macro number %i definition = %s in: %p out: %p\n", index, macro->def, input, *output);
  }
  
  /* buffer for definition or builtin output */
  out =  alloc_io_buffer(init_size_processbuf);
  out->file = -1; /* so not real output, but a memory buffer */
  out->position = 0;
  
  
  /* buffer for argument output */
  /* some performance optimisation with predetermined size */
  // if((in->file < 0) && (in->size > init_size_processbuf))
  // {
    /* the input is not a file, but a buffer bigger than the default buffer size */
    // arg_out =  alloc_io_buffer(in->size);
    // fprintf(stderr," extra arg buf size: %i\n", in->size);
  // }
  // else
  // {
    arg_out =  alloc_io_buffer(init_size_processbuf);
  // }
  arg_out->file = -1; /* so not real output, but a memory buffer */
  arg_out->position = 0;
  arg_out->divnum = (*output)->divnum;
  
  
  /* set the stack for local access */
  start_local_stacks();
  
  /* copy the name of the macro to first position on stack */ 
  macro_size = macro_len - macro->post_size - macro->pre_size;
  
  push_text(0, output, (*output)->position - macro_len + macro->pre_size + 1, macro_size);
  
  macro_depth++;
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
    input->position -= macro->post_size - 1; /* next char in input */
    
    if(input->position >= input->length)
    {
      if(input->file >= 0)
      {
        /* input is file */
        /* read the next part of the input file */
        read_input(input, max_size_macro);
      }
      /* else end of input memory buffer, this should not happen */
    }
    
    /* for proper line counting */
    line_count_flag = -macro->post_size;
    
    /* trick to skip macro expansion in the first number of chars */
    if(macro->arg_type == Run_macro_yes)
    {
      process_input(input, &arg_out, macro->arg_type + macro->post_size);
    }
    else
    {
      process_input(input, &arg_out, macro->arg_type);
    }
    
    // in = *input;
    
    input->position--; /* position 1 back because it will be incremented in process_input */
    
  }
  else
  {
    if(debug)
    {
      printf(" no argument parsing \n");
    }
    /* need to put last part of macro back in input */
    input->position -= macro->post_size; 
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
    flush_output(*output);
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
      process_input(out, output, Run_macro_yes);
    
  }
  else
  {
    /* the input out could be a file input
     * this needs to be handled differently from a memory buffer
     */ 
    if(out->file >= 0)
    {
      // fprintf(stderr,"step 1, pos: %i, len: %i\n", out->position, out->length);
      *output = putchars_buffer(&(out->data[out->position]), out->length - out->position, *output);
      // fprintf(stderr,"step 2, pos: %i, len: %i\n", out->position, out->length);
 
      while( read_input(out, 0) > 0)
      {
      // fprintf(stderr,"step a, pos: %i, len: %i\n", out->position, out->length);
        *output = putchars_buffer(out->data, out->length, *output);
      // fprintf(stderr,"step b, pos: %i, len: %i\n", out->position, out->length);
      }
  
    }
    else
    {
      /* copy the result to output */
      // fprintf(stderr,"step 11, pos: %i, len: %i\n", out->position, out->length);
      *output = putchars_buffer(&(out->data[out->position]), out->length - out->position, *output);
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


static inline void run_pattern(int macro_num, int *arg_level,  pattern_masks *masks, pattern_vectors (*vec), pattern_registers *prev_patcheck, pattern_registers *patcheck, uint8_t inp, uint8_t prev_inp, data_buffer *buffer, data_buffer **output, arg_run *stop)
{
  uint64_t arg_result;
  int j,
      vecnum;
  
  if(arglist != NULL)
  {
    if(macro_num >= 0)
    {
      *arg_level = 1; /* set argument level */
    }

    /* second part bitap */
    for(vecnum = (arglist->vec_size - 1); vecnum > 0; vecnum--)
    {
      
      patcheck[vecnum].mem = prev_patcheck[vecnum].mem | prev_patcheck[vecnum].check;
      patcheck[vecnum].mem &= (*vec)[vecnum][prev_inp];
      patcheck[vecnum].mem &= masks[vecnum].starmask;
      
      patcheck[vecnum].check = prev_patcheck[vecnum].check << 1;
      if(prev_patcheck[vecnum - 1].check & (0b1ULL << 63))
      {
        patcheck[vecnum].check |= 0b1ULL;
      }
    }
    
    patcheck[0].mem = prev_patcheck[0].mem | prev_patcheck[0].check;
    patcheck[0].mem &= (*vec)[0][prev_inp];
    patcheck[0].mem &= masks[0].starmask;
    
    patcheck[0].check = prev_patcheck[0].check << 1;

    
    /* the higher > 0 checks are done first for carry bits */
    for(vecnum = (arglist->vec_size - 1); vecnum > 0; vecnum--)
    {
      /* bitap check for zero characters */
      patcheck[vecnum].check |= (patcheck[vecnum].check & masks[vecnum].zeromask) << 1;
      if((patcheck[vecnum - 1].check & masks[vecnum - 1].zeromask) & (0b1ULL << 63))
      {
        patcheck[vecnum].check |= 0b1ULL;
      }
      /* initialise the first character of a search word */
      patcheck[vecnum].check |= masks[vecnum].init;
      /* bitap check for one or more characters */
      patcheck[vecnum].check |= (patcheck[vecnum].mem << 1);
      if(patcheck[vecnum - 1].mem & (0b1ULL << 63))
      {
        patcheck[vecnum].check |= 0b1ULL;
      }
      /* (the normal) bitap check for the current character */
      patcheck[vecnum].check &= (*vec)[vecnum][inp];
      
      /* copy the masks */
      patcheck[vecnum].onetimemask = prev_patcheck[vecnum].onetimemask;
    }

    
    /* then the lowest check */
    /* bitap check for zero characters */
    patcheck[0].check |= (patcheck[0].check & masks[0].zeromask) << 1;
    /* initialise the first character of a search word */
    patcheck[0].check |= masks[0].init;
    /* bitap check for one or more characters */
    patcheck[0].check |= (patcheck[0].mem << 1);
    /* (the normal) bitap check for the current character */
    patcheck[0].check &= (*vec)[0][inp];     

    /* copy the masks */
    patcheck[0].onetimemask = prev_patcheck[0].onetimemask;
    
    
    /* check to see if the patterns are found after the one time check */
    for(vecnum = 0; vecnum < arglist->vec_size; vecnum++)
    {
      arg_result = patcheck[vecnum].check & patcheck[vecnum].onetimemask;
      
      if(arg_result != 0LL)
      {
        j = 0;
        while((j < masks[vecnum].masks_end) && (stop->status == arg_continu))
        {
          if(((masks[vecnum].masks[j] & arg_result) != 0LL) && (masks[vecnum].masks_run_level[j] >= *arg_level))
          {
            /* found the trigger */
            if(masks[vecnum].masks_run_patlen[j] < 0)
            {
              *stop = exec_program(masks[vecnum].masks_run[j], -(masks[vecnum].masks_run_patlen[j]), current_status_pattern, output);
              patcheck[vecnum].onetimemask &= ~(masks[vecnum].masks[j]);
            }
            else
            {
              *stop = exec_program(masks[vecnum].masks_run[j], masks[vecnum].masks_run_patlen[j], current_status_pattern, output);
            }
          }
          j++;
        }
      }
    }
    
    if(stop->status == arg_abort)
    {
      /* no argument collection, position input should be placed back
       * output is a buffer (during argument collection)
       * thus output->position can be used to determine number of bytes
       */
      buffer->position -= (*output)->position + 1;
      his.position -= (*output)->position;
    }
  }
}


static inline void run_update_positions(data_buffer *buffer, data_buffer **output)
{
  (*output)->position++;
  
  if((*output)->position >= (*output)->size)
  {
    if((*output)->file >= 0)
    {
      /* output to real file or stdout */
      write_output(*output, max_size_macro);
    }
    else
    {
      /* output is a memory buffer that needs to grow */
      *output = incr_buffer(*output, add_size_processbuf);
    }
  }
  
  buffer->position++;
  if(buffer->position >= buffer->length)
  {
    if(buffer->file >= 0)
    {
      /* input is file */
      /* read the next part of the input file */
      read_input(buffer, max_size_macro);
      
      if(debug)
      {
        printf("\n\n ---------- \n size: %i  pos: %i \n -------------\n", buffer->length, buffer->position);
      }
    }
    /* else end of input memory buffer, this will end the while loop */
  }
  
}



static inline void run_macros(run_macro *runmacro, int macro_num, int *arg_level, int len, data_buffer *input, data_buffer **output)
{
  if((macro_num >= 0) && (*runmacro == Run_macro_yes))
  {
    exec_macro(macro_num, len + 1, input, output);
    
    *arg_level = 0; /* reset argument level */
    
    /* counting macros just for statistics */
    statistics[len + 1]++;
  }
  
  /* trick to skip running macro for a first number of times */
  if(*runmacro > Run_macro_yes)
  {
    (*runmacro)--;
  }
}


static inline void run_count_lines(uint8_t inp, data_buffer *buffer)
{
  /* count lines */
  if((inp == '\n') && (buffer->file >= 0) && (line_count_flag >= 0))
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



static inline void run_bitap_macro(run_macro runmacro, uint8_t inp, int *len, int *macro_num, uint64_t *prev_check, uint64_t *current_check)
{
  uint64_t result[3];
  // int j;
  
 
  if(runmacro >= Run_macro_yes)
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
    
    if(runmacro == Run_macro_yes)
    {
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
            }   
            
            
            if(debug)
            {
              printf("len: %i  macronum: %i", *len, *macro_num);
            }
            
            (*len)--;
          }
        }
      }
    }
    
  }
   
}


void process_input(data_buffer *buffer, data_buffer **output, run_macro runmacro)
{
  pattern_vectors (*vec);
  pattern_masks *masks;
  uint64_t *prev_check,
           *current_check;
  pattern_registers *prev_patcheck,
                    *patcheck;
  int len,
      arg_level;
  int macro_num;
  arg_run stop;
  uint8_t inp,
          prev_inp;
  
  
  /* stop is used to end the argument collection and thereby also this function call */
  stop.status = arg_continu;

  arg_level = 0;

  /* setting up the pointers for the argument collection */
  if(arglist != NULL)
  {
    vec = arglist->vec;
    masks = arglist->masks;
  }
  else
  {
    /* these are not used in this case */
    vec = NULL;
    masks = NULL;
  }

  
  if(debug)
  {
    current_check = his_checks + his_index[his.position];
    printf(" in: %p, pos: %i, size: %i, length %i \n", buffer, buffer->position, buffer->size, buffer->length);
    printf(" out: %p, pos: %i, size: %i, length %i \n", (*output), (*output)->position, (*output)->size, (*output)->length);
    printf(" check: %p, 0: %lu, 1: %lu, 2: %lu \n", current_check, current_check[0], current_check[1], current_check[2]);
    printf(" arglist: %p, cur_stat: %p \n", arglist, current_status_pattern);
    printf("\n his position: %i \n\n", his.position);
  }
  

  
  while((buffer->position < buffer->length) && (stop.status == arg_continu))
  {
    /* reset macro number */
    macro_num = -1;
    
    /* set the checks in history array correct: macros can have changed the position 
     * the step_history can also reduce the position 
     */
    current_check = step_history();
    prev_check = his_checks + his_index[his.position - 1];;
    prev_inp = his_inchar[his.position - 1];
    
    patcheck = (pattern_registers *)(current_check + his.incr_checks);
    prev_patcheck = (pattern_registers *)(prev_check + his.incr_checks);
    
    /* the current input byte */
    inp = buffer->data[buffer->position];
    
    /* copy input to output */
    (*output)->data[(*output)->position] = inp;
    
    /* copy input to history buffer: necessary for complete macro name in call to macros */
    his_inchar[his.position] = inp;
    
    /* bitap algo for finding macros */
    run_bitap_macro(runmacro, inp, &len, &macro_num, prev_check, current_check);   
    
    /* bitap algo for argument collection */
    run_pattern(macro_num, &arg_level, masks, vec, prev_patcheck, patcheck, inp, prev_inp, buffer, output, &stop);
    
    if(debug)
    {
      print_history();
    }
    /* counting lines */
    run_count_lines(inp, buffer);
    
    /* run macro if applicable */
    run_macros(&runmacro, macro_num, &arg_level, len, buffer, output);
    
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
    printf("leaving processor function posinp: %i posout: %i fileout: %i\n", buffer->position, (*output)->position, (*output)->file);
  }
}



void process_virtual(uint8_t inp)
{
  pattern_vectors (*vec);
  pattern_masks *masks;
  uint64_t *prev_check,
           *current_check;
  pattern_registers *prev_patcheck,
                    *patcheck;
  int len;
  int macro_num = -1;
  uint8_t prev_inp;
  int /*j,*/
      vecnum;


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
  run_bitap_macro(Run_macro_yes, inp, &len, &macro_num, prev_check, current_check);   

  /* bitap algo for argument collection */
  
  if(arglist != NULL)
  {
    /* setting up the pointers for the argument collection */
    vec = arglist->vec;
    masks = arglist->masks;

    /* second part bitap */
    for(vecnum = (arglist->vec_size - 1); vecnum > 0; vecnum--)
    {
      
      patcheck[vecnum].mem = prev_patcheck[vecnum].mem | prev_patcheck[vecnum].check;
      patcheck[vecnum].mem &= (*vec)[vecnum][prev_inp];
      patcheck[vecnum].mem &= masks[vecnum].starmask;
      
      patcheck[vecnum].check = prev_patcheck[vecnum].check << 1;
      if(prev_patcheck[vecnum - 1].check & (0b1ULL << 63))
      {
        patcheck[vecnum].check |= 0b1ULL;
      }
    }
    
    patcheck[0].mem = prev_patcheck[0].mem | prev_patcheck[0].check;
    patcheck[0].mem &= (*vec)[0][prev_inp];
    patcheck[0].mem &= masks[0].starmask;
    
    patcheck[0].check = prev_patcheck[0].check << 1;

    
    /* the higher > 0 checks are done first for carry bits */
    for(vecnum = (arglist->vec_size - 1); vecnum > 0; vecnum--)
    {
      /* bitap check for zero characters */
      patcheck[vecnum].check |= (patcheck[vecnum].check & masks[vecnum].zeromask) << 1;
      if((patcheck[vecnum - 1].check & masks[vecnum - 1].zeromask) & (0b1ULL << 63))
      {
        patcheck[vecnum].check |= 0b1ULL;
      }
      /* initialise the first character of a search word */
      patcheck[vecnum].check |= masks[vecnum].init;
      /* bitap check for one or more characters */
      patcheck[vecnum].check |= (patcheck[vecnum].mem << 1);
      if(patcheck[vecnum - 1].mem & (0b1ULL << 63))
      {
        patcheck[vecnum].check |= 0b1ULL;
      }
      /* (the normal) bitap check for the current character */
      patcheck[vecnum].check &= (*vec)[vecnum][inp];
      
      /* copy the masks */
      patcheck[vecnum].onetimemask = prev_patcheck[vecnum].onetimemask;
    }

    
    /* then the lowest check */
    /* bitap check for zero characters */
    patcheck[0].check |= (patcheck[0].check & masks[0].zeromask) << 1;
    /* initialise the first character of a search word */
    patcheck[0].check |= masks[0].init;
    /* bitap check for one or more characters */
    patcheck[0].check |= (patcheck[0].mem << 1);
    /* (the normal) bitap check for the current character */
    patcheck[0].check &= (*vec)[0][inp];     

    /* copy the masks */
    patcheck[0].onetimemask = prev_patcheck[0].onetimemask;
  }  

  
}
