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
#include <limits.h>
#include "definesizes.h"
#include "exitcodes.h"
#include "xmalloc.h"
#include "sds.h"
#include "input.h"
#include "output.h"
#include "bitapvec.h"

#include "stack.h"


/* using 1 stack at the end to hold constant variables */
#define number_of_var_stacks 1
#define number_of_total_stacks  (number_of_default_stacks + number_of_var_stacks)
/* the stack for constant variables is placed at the end */
#define const_var_stack number_of_default_stacks


/* the structs for the stack */
typedef enum
{
    buffer_pointer,
    str_pointer,
    str_sds,
    str_sds_const,
    number
} stack_type;

typedef union
{
  int64_t num;
  sds str;
  uint8_t *str_p;
  data_buffer **buf_p;
} stack_value;


typedef struct
{
    stack_type type;   /* type of value */
    int size;          /* size or length of the string */ 
    int start;         /* start of the string in type buffer_pointer */
    stack_value value; /* the value: either buffer pointer, pointer, sds string or int number */
} stack_entry;
    
    
typedef struct
{
    int stack_size,     /* reserved size of the stack */
        stack_end;      /* end of the current stack */
    stack_entry st[];
} stack;

stack *default_st[number_of_total_stacks];

typedef struct
{
  int command;
  int option;
  int rank;         
} op_stack_entry;
    
    
typedef struct
{
    int stack_size,     /* reserved size of the stack */
        stack_end;      /* end of the current stack */
    op_stack_entry st[];
} op_stack;

op_stack *default_op_st[number_of_default_stacks];



/* the stack status for making a stack for a current function */
typedef struct stack_status
{
  int start[number_of_default_stacks];
  int start_op[number_of_default_stacks];
  int active_stack;
  struct stack_status *prev;
} stack_status;

stack_status *st_status = NULL;


/* struct and variables for stack commands 
 * similar to macro list
 */
typedef union
{
  uint8_t  n8[8];
  uint64_t n64;
} command_name;

typedef struct
{
  command_name name;
  int (*command)(int, int, status_pattern *, data_buffer **, arg_run *);  /* a pointer to the command function */
  int option;
} command_entry;

const command_entry command_list[] =
{
  {{"abort   "}, &st_nop, 0},      /* [0] */
  {{"stop    "}, &st_nop, 0},      /* [1] */
  {{"0pushstr"}, &st_pushvar, 0},  /* [2] */
  {{"0pushnum"}, &st_pushvar, 0},  /* [3] */
  {{"if      "}, &st_if, 0},       /* [4] */
  {{"else    "}, &st_else, 0},     /* [5] */
  {{"endif   "}, &st_nop, 0},      /* [6] */
  {{"nop     "}, &st_nop, 0},
  {{"pop     "}, &st_pop, 0},
  {{"dup     "}, &st_dup, 0},
  {{"pop_to_0"}, &st_pop_to, 0},
  {{"pop_to_1"}, &st_pop_to, 1},
  {{"pop_to_2"}, &st_pop_to, 2},
  {{"pop_to_3"}, &st_pop_to, 3},
  {{"pop_to_4"}, &st_pop_to, 4},
  {{"begin   "}, &st_beginarg, 0},
  {{"begin-1 "}, &st_beginarg, -1},
  {{"begin-2 "}, &st_beginarg, -2},
  {{"begin-3 "}, &st_beginarg, -3},
  {{"begin-4 "}, &st_beginarg, -4},
  {{"begin+0 "}, &st_beginarg, 1},
  {{"begin+1 "}, &st_beginarg, 2},
  {{"begin+2 "}, &st_beginarg, 3},
  {{"end     "}, &st_endarg, 2},
  {{"end-0   "}, &st_endarg, 1},
  {{"end-1   "}, &st_endarg, 0},
  {{"end-2   "}, &st_endarg, -1},
  {{"end-3   "}, &st_endarg, -2},
  {{"end-4   "}, &st_endarg, -3},
  {{"argpos  "}, &st_argposition, 0},
  {{"argnum  "}, &st_argnumber, 0},
  {{"=0      "}, &st_cmp_num, 0},
  {{"=1      "}, &st_cmp_num, 1},
  {{"=2      "}, &st_cmp_num, 2},
  {{"=3      "}, &st_cmp_num, 3},
  {{">0      "}, &st_greater_num, 0},
  {{">1      "}, &st_greater_num, 1},
  {{">2      "}, &st_greater_num, 2},
  {{">3      "}, &st_greater_num, 3},
  {{">=0     "}, &st_greaterequal_num, 0},
  {{">=1     "}, &st_greaterequal_num, 1},
  {{">=2     "}, &st_greaterequal_num, 2},
  {{">=3     "}, &st_greaterequal_num, 3},
  {{"<0      "}, &st_smaller_num, 0},
  {{"<1      "}, &st_smaller_num, 1},
  {{"<2      "}, &st_smaller_num, 2},
  {{"<3      "}, &st_smaller_num, 3},
  {{"<=0     "}, &st_smallerequal_num, 0},
  {{"<=1     "}, &st_smallerequal_num, 1},
  {{"<=2     "}, &st_smallerequal_num, 2},
  {{"<=3     "}, &st_smallerequal_num, 3},
  {{"=       "}, &st_compare_number, 0},
  {{"!=      "}, &st_compare_number, 1},
  {{">       "}, &st_compare_number, 2},
  {{"<       "}, &st_compare_number, 3},
  {{">=      "}, &st_compare_number, 4},
  {{"<=      "}, &st_compare_number, 5},
  {{"+       "}, &st_add, 1},
  {{"-       "}, &st_add, -1},
  {{"mod     "}, &st_modulo, 0},
  {{"/       "}, &st_divide, 0},
  {{"*       "}, &st_multiply, 0},
  {{"power   "}, &st_power, 0},
  {{"~       "}, &st_bit_not, 0},
  {{"!       "}, &st_log_not, 0},
  {{"shift_l "}, &st_shift_left, 0},
  {{"<<1     "}, &st_shift_left, 1},
  {{"shift_r "}, &st_shift_right, 0},
  {{">>1     "}, &st_shift_right, 1},
  {{"bit_and "}, &st_bit_logic, 0},
  {{"bit_or  "}, &st_bit_logic, 1},
  {{"bit_exor"}, &st_bit_logic, 2},
  {{"and     "}, &st_logic, 0},
  {{"or      "}, &st_logic, 1},
  {{"opexif< "}, &st_op_stack_ex_if, 0},
  {{"opexif<="}, &st_op_stack_ex_if, 1},
  {{"opexif> "}, &st_op_stack_ex_if, 2},
  {{"opexif>="}, &st_op_stack_ex_if, 3},
  {{"opexall "}, &st_op_stack_ex, 0},
  {{"opexto  "}, &st_op_stack_ex_to, 0},
  {{"oppush  "}, &st_op_stack_push, 0},
  {{"strcmp  "}, &st_cmp_string, 0},
  {{"ifcmpset"}, &st_if_cmp_set, 0},
  {{"ifthen  "}, &st_ifthen, 0},
  {{"stack_a "}, &st_setstack, 0},
  {{"stack_b "}, &st_setstack, 1},
  {{"stack_c "}, &st_setstack, 2},
  {{"stack_d "}, &st_setstack, 3},
  {{"stack_e "}, &st_setstack, 4},
  {{"stack_f "}, &st_setstack, 5},
  {{"stack_g "}, &st_setstack, 6},
  {{"stack_h "}, &st_setstack, 7},
  {{"putarg0 "}, &st_push_toarg, 0},
  {{"putarg1 "}, &st_push_toarg, 1},
  {{"putarg2 "}, &st_push_toarg, 2},
  {{"putarg3 "}, &st_push_toarg, 3},
  {{"putarg4 "}, &st_push_toarg, 4},
  {{"putarg5 "}, &st_push_toarg, 5},
  {{"putarg6 "}, &st_push_toarg, 6},
  {{"putarg7 "}, &st_push_toarg, 7},
  {{"putarg8 "}, &st_push_toarg, 8},
  {{"putarg9 "}, &st_push_toarg, 9},
  {{"getarg  "}, &st_get_arg_num, 0},
  {{"getarg0 "}, &st_get_arg, 0},
  {{"getarg1 "}, &st_get_arg, 1},
  {{"getarg2 "}, &st_get_arg, 2},
  {{"getarg3 "}, &st_get_arg, 3},
  {{"getarg4 "}, &st_get_arg, 4},
  {{"getarg5 "}, &st_get_arg, 5},
  {{"getarg6 "}, &st_get_arg, 6},
  {{"getarg7 "}, &st_get_arg, 7},
  {{"getarg8 "}, &st_get_arg, 8},
  {{"getarg9 "}, &st_get_arg, 9},
  {{"get_st  "}, &st_get_from_out_opt, 0},
  {{"get_st+1"}, &st_get_from_out_opt, -1},
  {{"get_st+2"}, &st_get_from_out_opt, -2},
  {{"get_st+3"}, &st_get_from_out_opt, -3},
  {{"getlast1"}, &st_get_from_out_opt, 1},
  {{"getlast2"}, &st_get_from_out_opt, 2},
  {{"getlast3"}, &st_get_from_out_opt, 3},
  {{"getlast4"}, &st_get_from_out_opt, 4},
  {{"getlast5"}, &st_get_from_out_opt, 5},
  {{"putoutst"}, &st_replace_out_start, 0},
  {{"putout  "}, &st_replace_out, 0},
  {{"putout-0"}, &st_replace_out_opt, 0},
  {{"putout-1"}, &st_replace_out_opt, 1},
  {{"putout-2"}, &st_replace_out_opt, 2},
  {{"putout-3"}, &st_replace_out_opt, 3},
  {{"putout-4"}, &st_replace_out_opt, 4},
  {{"putout-5"}, &st_replace_out_opt, 5},
  {{"copyto_a"}, &st_copy, 0},
  {{"copyto_b"}, &st_copy, 1},
  {{"copyto_c"}, &st_copy, 2},
  {{"copyto_d"}, &st_copy, 3},
  {{"copyto_e"}, &st_copy, 4},
  {{"copyto_f"}, &st_copy, 5},
  {{"copyto_g"}, &st_copy, 6},
  {{"copyto_h"}, &st_copy, 7},
  {{"cat     "}, &st_cat, 0},
  {{"strlen  "}, &st_strlen, 0},
  {{"str*    "}, &st_str_multiply, 0},
  {{"set_base"}, &st_setbase, 0},
  {{"base_=_1"}, &st_base_option, 1},
  {{"nooverr "}, &st_overrule, 0},
  {{"recur_y "}, &st_overrule, 1},
  {{"recur_n "}, &st_overrule, 2},
};

const int num_commands = sizeof(command_list) / sizeof(command_entry);

short int asciitoradix[256];


typedef struct
{
  int command;
  int option;
} program_entry;

program_entry (*program_list);

int size_program_list,
    end_program_list;


int empty_string_on_stack;
    
void init_program_list(void)
{
  
  program_list = xmalloc(sizeof(program_entry) * init_size_program_list);
  
  size_program_list = init_size_program_list;
  end_program_list = 0;
      
}

void increase_program_list(void)
{
   if(end_program_list >= size_program_list)
    {
      /* increase program_list size */
      program_list = xrealloc(program_list, sizeof(program_entry) * (size_program_list + add_size_program_list));
  
      size_program_list += add_size_program_list;
    }
}


void init_asciitoradix(void)
{
  int i;
    
  for(i = 0; i < 256; i++)
  {
    asciitoradix[i] = -256;
  }
  
  for(i = 0; i <= 9; i++)
  {
    asciitoradix['0' + i] = i;
  }

  for(i = 10; i <= 36; i++)
  {
    asciitoradix['a' - 10 + i] = i;
    asciitoradix['A' - 10 + i] = i;
  }
 
}


int find_command(uint8_t *name, int len)
{
  command_name search;
  int i,
      ret = -1;
  
  /* copy name to search word */
  for(i = 0; i < len; i++)
  {
    search.n8[i] = name[i];
  }
  /* and fill rest with space */
  for(; i < 8; i++)
  {
    search.n8[i] = ' ';
  }
  
  i = 0;
  
  while( (i < num_commands) && (ret < 0))
  {
    if(search.n64 == command_list[i].name.n64)
    {
      /* found command */
      ret = i;
    }
    i++;
  }
  
  return(ret);
}


int str_to_commands(uint8_t *str, int length)
{
  int start,
      start_str,
      i,
      command;
  long long int number;
  uint8_t in;
  
  
  start = end_program_list;
  
  i = 0;
  

  while(i < length)
  {
    
    /* find first non space char */
    while(((str[i] == ' ') || (str[i] == '\n')) && (i < length))
    {
      // printf("-%c-%i+",str[i], i);
      i++;
    }

    /* the first char */
    in = str[i];
    if(in == '\"')   /* " is start of a string */
    {
      i++;
      start_str = i;
      /* find end of string */
      while(!((str[i] == '\"') && ((str[i+1] == ' ') || (str[i+1] == '\n'))) && (i < length - 1))
      {
        // printf("+%c+%i-",str[i], i);
        i++;
      }

      /* set command */
      (program_list[end_program_list]).command = 2; /* 2 = push constant string to stack */
      (program_list[end_program_list]).option = default_st[const_var_stack]->stack_end; /* the option is an index to the place in the var stack */

      /* push found string to constant var stack */
      push_str(const_var_stack,  &(str[start_str]), i - start_str);
    }
    else
    {
      if(((in >= '0') && (in <= '9')) || ((in == '-') && (str[i+1] >= '0') && (str[i+1] <= '9'))) /* numbers start with a digit or - */
      {
        number = 0LL;
        
        if(in == '-')
        {
          i++;
        }

        while((str[i] >= '0') && (str[i] <= '9') && (i < length))
        {
          number *= 10;
          number += str[i] - '0';

          // printf("=%c=%i+",str[i], i);
          i++;
        }

        if(in == '-')
        {
          number = -number;
        }

        /* set command */
        (program_list[end_program_list]).command = 3; /* 3 = push constant number to stack */
        (program_list[end_program_list]).option = default_st[const_var_stack]->stack_end; /* the option is an index to the place in the var stack */

        /* push number to constant var stack */
        push_num(const_var_stack,  number);

      }
      else     /* otherwise it is a command */
      {
        start_str = i;
        /* find end of command */
        while((str[i] != ' ') && (str[i] != '\n') && (i < length))
        {
          // printf("!%c!%i+",str[i], i);
          i++;
        }
        command = find_command(&(str[start_str]), i - start_str);
        if(command >= 0)
        {
          (program_list[end_program_list]).command = command;
          (program_list[end_program_list]).option = command_list[command].option;
          // printf(" new command: %i\n", command);
        }
        else
        {
          /* error in string */
          fprintf(stderr, "Error line: %i in file: %s line: %i; command: ", line_counter, current_input_file_buffer->filename, local_line_counter);
          fwrite(&(str[start_str]), 1, i - start_str, stderr);
          fprintf(stderr, " is not known.\n");
          exit_code = Exit_user; 
          /* no exit, need to fix increment */
          end_program_list--;
        }

      }
    }
    // printf("current end_program_list: %i\n", end_program_list);
    i++;
    end_program_list++;
    increase_program_list();
  }

  if(end_program_list > start)
  {
    /* add end to program */
    (program_list[end_program_list]).command = -1; /* < 0 means end of program */
    (program_list[end_program_list]).option = 0;
    end_program_list++;
    increase_program_list();
    
    return(start);
  }
  else
  {
    return(-1); /* no program */
  }

}


stack *init_stack(void)
{
  stack *new;
  size_t total;
  
  total = sizeof(stack) + init_size_stack * sizeof(stack_entry);
  
  new = xmalloc(total);
  
  new->stack_size = init_size_stack;
  new->stack_end = 0;
  
  return(new);
}

stack *increase_stack(stack *old)
{
  size_t total;
  
  if(old->stack_end >= old->stack_size)
  {
    total = sizeof(stack) + ((old->stack_size + add_size_stack) * sizeof(stack_entry));

    /* increase size */
    old = xrealloc(old, total);
    
    old->stack_size += add_size_stack;
  }
  
  return(old);
}

op_stack *init_op_stack(void)
{
  op_stack *new;
  size_t total;
  
  total = sizeof(op_stack) + init_size_stack * sizeof(op_stack_entry);
  
  new = xmalloc(total);
  
  new->stack_size = init_size_stack;
  new->stack_end = 0;
  
  return(new);
}

op_stack *increase_op_stack(op_stack *old)
{
  size_t total;
  
  if(old->stack_end >= old->stack_size)
  {
    total = sizeof(op_stack) + ((old->stack_size + add_size_stack) * sizeof(op_stack_entry));

    /* increase size */
    old = xrealloc(old, total);
    
    old->stack_size += add_size_stack;
  }
  
  return(old);
}



void init_stacks(void)
{
  int i;
  
  for(i = 0; i < number_of_total_stacks; i++)
  {
    default_st[i] = init_stack();
  }

  for(i = 0; i < number_of_default_stacks; i++)
  {
    default_op_st[i] = init_op_stack();
  }

  /* put an empty string on the variable stack at the first position */
  empty_string_on_stack = default_st[const_var_stack]->stack_end; // push_sds(const_var_stack, "", 0);
  default_st[const_var_stack]->st[default_st[const_var_stack]->stack_end].value.str = sdsnewlen("", 0);
  default_st[const_var_stack]->st[default_st[const_var_stack]->stack_end].size = 0;
  default_st[const_var_stack]->st[default_st[const_var_stack]->stack_end].type = str_sds;
  
  
  default_st[const_var_stack]->stack_end++;
  
  
}

void start_local_stacks(void)
{
  int i;
  stack_status *new;
  
  new = xmalloc(sizeof(stack_status));
  
  for(i = 0; i < number_of_default_stacks; i++)
  {
    new->start[i] = default_st[i]->stack_end;
    new->start_op[i] = default_op_st[i]->stack_end;
  }
  
  new->active_stack  = 0;
  
  new->prev = st_status;
  
  st_status = new;
  
}

void change_arg_stack_start(int step)
{
   st_status->start[0] += step;
}


void pop_stack(int stnum)
{
  stack *st;

  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];

  if(st->stack_end > 0)
  {
    st->stack_end--;

    if(debug_stack)
    {
      printf(" pop stack %i, end %i\n", stnum, st->stack_end); 
    }
    
    if(st->st[st->stack_end].type == str_sds)
    {
      sdsfree(st->st[st->stack_end].value.str);
    }
    
  }
}



void end_local_stacks(void)
{
  int i;
  stack_status *prev;
  

  if(st_status != NULL)
  {
    prev = st_status->prev; 

    for(i = 0; i < number_of_default_stacks; i++)
    {
      if(debug_stack)
      {
        printf("clearing stack %i: [%d, %d]\n", i,  st_status->start[i], default_st[i]->stack_end);
      }
      
      while(default_st[i]->stack_end > st_status->start[i])
      {
        pop_stack(i);
      }
      
      default_op_st[i]->stack_end = st_status->start_op[i];

    }

    xfree(st_status);
    
    st_status = prev;
  }
 
}


int get_size_stack(int stnum)
{
  int num;

  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal;
    stnum = 0;
  }

  num = default_st[stnum]->stack_end - st_status->start[stnum];

  return(num);
}


int push_text(int stnum, data_buffer **text, int start, int len)
{
  stack *st;

  if((stnum < 0) || (stnum >= number_of_total_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];

  if(debug_stack)
  {
    printf(" start stack = %i, push text  '%.*s' to stack %i, end %i\n", st_status->start[stnum], len, &((*text)->data[start]), stnum, st->stack_end); 
  }
  
  if(len == 0)
  {
    /* put the empty string instead of requested */
    st->st[st->stack_end].value.str = default_st[const_var_stack]->st[empty_string_on_stack].value.str;
    st->st[st->stack_end].size = len;
    st->st[st->stack_end].start = 0;
    st->st[st->stack_end].type = str_sds_const;
  }
  else
  {
    st->st[st->stack_end].value.buf_p = text;
    st->st[st->stack_end].size = len;
    st->st[st->stack_end].start = start;
    st->st[st->stack_end].type = buffer_pointer;
  }
  
  st->stack_end++;
  
  default_st[stnum] = increase_stack(st);

  return(st->stack_end - 1);
}

int push_str(int stnum, uint8_t *text, int len)
{
  stack *st;

  if((stnum < 0) || (stnum >= number_of_total_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];

  if(debug_stack)
  {
    printf(" push string '%.*s' to stack %i, end %i\n", len, text, stnum, st->stack_end); 
  }

  if(len == 0)
  {
    /* put the empty string instead of requested */
    st->st[st->stack_end].value.str = default_st[const_var_stack]->st[empty_string_on_stack].value.str;
    st->st[st->stack_end].size = len;
    st->st[st->stack_end].type = str_sds_const;
  }
  else
  {
    st->st[st->stack_end].value.str = sdsnewlen(text, len);
    st->st[st->stack_end].size = len;
    st->st[st->stack_end].type = str_sds;
  }
  
  st->stack_end++;
  
  default_st[stnum] = increase_stack(st);
  
  return(st->stack_end - 1);
}


int push_sds(int stnum, sds text)
{
  stack *st;

  if((stnum < 0) || (stnum >= number_of_total_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];

  if(debug_stack)
  {
    printf(" push sds %s to stack %i, end %i\n", text, stnum, st->stack_end); 
  }
  
  st->st[st->stack_end].value.str = text;
  st->st[st->stack_end].size = sdslen(text);
  st->st[st->stack_end].type = str_sds;
  
  st->stack_end++;
  
  default_st[stnum] = increase_stack(st);
  
  return(st->stack_end - 1);
}


int push_num(int stnum, long long int num)
{
  stack *st;

  if((stnum < 0) || (stnum >= number_of_total_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];

  if(debug_stack)
  {
    printf(" push number '%lld' to stack %i, end %i\n", num, stnum, st->stack_end); 
  }
  
  st->st[st->stack_end].value.num = num;
  st->st[st->stack_end].type = number;
  
  st->stack_end++;
  
  default_st[stnum] = increase_stack(st);

  return(st->stack_end - 1);
}

long long int str_to_num(uint8_t *str, int len)
{
  long long int ret;
  int pos,
      radix,
      neg;
  short int num;
  
  ret = 0ll;
  pos = 0;
  
  /* eat spaces */    
  while(((*str == ' ') || (*str == '\t') || (*str == '\n')) && (pos < len))
  {
    str++;
    pos++;
  }    
  
  /* check sign */
  if((*str == '-') && (pos < len))
  {
    neg = -1;
    str++;
    pos++;
  }
  else
  {
    neg = 1;
    if((*str == '+') && (pos < len))
    {
      str++;
      pos++;
    }
  }
  
  /* check radix */
  if((*str == '0') && (pos < (len - 1)))
  {
    /* a number with radix */
    str++;
    pos++;
    
    switch(*str)
    {
      case 'b':
      case 'B':
        radix = 2;
        str++;
        pos++;
        break;
      case 'x':
      case 'X':
        radix = 16;
        str++;
        pos++;
        break;
      case 'r':
      case 'R':
        str++;
        pos++;
        /* first position for radix number */   
        if((*str >= '0') && (*str <= '9') && (pos < len))
        {
          radix = *str - '0';
          str++;
          pos++;
          /* second position for radix number */   
          if((*str >= '0') && (*str <= '9') && (pos < len))
          {
            radix *= 10;
            radix += *str - '0';
            str++;
            pos++;
          }
          /* check correct ending radix */
          if((*str == ':') && (pos < len))
          {
            str++;
            pos++;
          }
          else
          {
            /* error */
            ret = -1ll;
          }
        }
        else
        {
          /* error */
          ret = -1ll;
        }
        
        break;
      default:
        radix = 8;
    }
    if((radix < 2) || (radix > 36))
    {
      ret = -1ll;
    }
  }
  else
  {
    radix = 10;
  }
  
  if(ret >= 0)
  {
    ret = 0ll;

    while(pos < len)
    {
      num = asciitoradix[*str];
      if((num >= 0) && (num <= radix))
      {
        ret *= radix;
        ret += num;
      }
      else
      {
        /* stop */
        pos = len;
      }
      str++;
      pos++;
    }
    /* use sign */
    ret *= neg;

  }
  else
  {
    ret = 0ll;
  }
  
  return(ret);
}

    
long long int argument_num(int stnum, int pos)
{
  data_buffer *data;
  stack *st;
  int stack_pos;
  stack_type type;
  long long int ret;


  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];
    
  stack_pos = st_status->start[stnum] + pos;

  ret = 0ll; /* default returns 0 in case of error */
  
  if(stack_pos < st->stack_end)
  {
    type = st->st[stack_pos].type;
    
    switch(type)
    {
      case buffer_pointer:
        // ret = strtoll(st->st[stack_pos].value.str_p, NULL, 0);
        data = *(st->st[stack_pos].value.buf_p);
        ret = str_to_num(&(data->data[st->st[stack_pos].start]), st->st[stack_pos].size);
        break;
      case str_pointer:
        // ret = strtoll(st->st[stack_pos].value.str_p, NULL, 0);
        ret = str_to_num(st->st[stack_pos].value.str_p, st->st[stack_pos].size);
        break;
      case str_sds:
      case str_sds_const:
        // ret = strtoll(st->st[stack_pos].value.str, NULL, 0);
        ret = str_to_num(st->st[stack_pos].value.str, sdslen(st->st[stack_pos].value.str));
        break;
      case number:
        ret = st->st[stack_pos].value.num;
        break;
    }
  }
  
  if(debug_stack)
  {
    printf(" Get number from stack: %i position: %i = stack position: %i number = %lli \n", stnum, pos, stack_pos, ret);
  }
  
  return(ret);
}


arg_text_return argument_text(int stnum, int pos)
{
  data_buffer *data;
  arg_text_return ret;
  stack *st;
  int stack_pos;
  stack_type type;
  
  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];
    
  stack_pos = st_status->start[stnum] + pos;
  
  if(stack_pos < st->stack_end)
  {
    type = st->st[stack_pos].type;
    
    switch(type)
    {
      case buffer_pointer:
        data = *(st->st[stack_pos].value.buf_p);
        ret.str_p = &(data->data[st->st[stack_pos].start]);
        ret.length = st->st[stack_pos].size;
        break;
      case str_pointer:
        ret.str_p = st->st[stack_pos].value.str_p;
        ret.length = st->st[stack_pos].size;
        break;
      case str_sds:
      case str_sds_const:
        ret.str_p = st->st[stack_pos].value.str;
        ret.length = sdslen(st->st[stack_pos].value.str);
        break;
      case number:
        ret.str_p = sdsfromlonglong(st->st[stack_pos].value.num);
        st->st[stack_pos].value.str = ret.str_p;
        st->st[stack_pos].type = str_sds;
        ret.length = sdslen(st->st[stack_pos].value.str);
        // printf("\n p: %p, len: %i\n", st->st[stack_pos].value.str, ret.length);
        break;
    }

    if(debug_stack)
    {
      printf("return string from stack: '%.*s' from stack %i, end %i\n", ret.length, ret.str_p, stnum, st->stack_end); 
    }

  }
  else
  {
    /* stack entry in this position does not exist */
    ret.str_p = NULL;
    ret.length = -1;
  }
    
  return(ret);
}



long long int pop_num(int stnum)
{
  long long int ret = 0;
  int pos;
  
  pos =  default_st[stnum]->stack_end - st_status->start[stnum] - 1; 
  
  if(debug_stack)
  {
      printf(" Popping number from stack: %i position: %i\n", stnum, pos); 
  }

  if(pos >= 0)
  {
    ret =  argument_num(stnum, pos);
  
    pop_stack(stnum);
  }

  return(ret);
}

void swap_on_stack(int first_pos, int second_pos, int stnum)
{
  stack *st;
  stack_entry temp;


  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal;
    stnum = 0;
  }

  st = default_st[stnum];

  first_pos += st_status->start[stnum];
  second_pos += st_status->start[stnum];

  temp.type = st->st[first_pos].type;
  temp.size = st->st[first_pos].size;
  temp.start = st->st[first_pos].start;
  temp.value.str = st->st[first_pos].value.str;

  st->st[first_pos].type = st->st[second_pos].type;
  st->st[first_pos].size = st->st[second_pos].size;
  st->st[first_pos].start = st->st[second_pos].start;
  st->st[first_pos].value.str = st->st[second_pos].value.str;

  st->st[second_pos].type = temp.type;
  st->st[second_pos].size = temp.size;
  st->st[second_pos].start = temp.start;
  st->st[second_pos].value.str = temp.value.str;

}



void copy_stack(int from, int pos, int stnum)
{
  stack *st,
        *st_from;
  
  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  if((from < 0) || (from >= number_of_total_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 8.\n", line_counter, current_input_file_buffer->filename, local_line_counter, from);
    exit_code = Exit_internal; 
    from = 8;
  }

  st = default_st[stnum];
  st_from = default_st[from];
  
  if(st_from->st[pos].type == str_sds)
  {
    if(from == const_var_stack)
    {
      st->st[st->stack_end].type = str_sds_const;
      st->st[st->stack_end].size = st_from->st[pos].size;
      st->st[st->stack_end].value.str = st_from->st[pos].value.str;
    }
    else
    {
      st->st[st->stack_end].type = str_sds;
      st->st[st->stack_end].size = st_from->st[pos].size;
      st->st[st->stack_end].value.str = sdsdup(st_from->st[pos].value.str);
    }  
      
  }
  else
  {  
    st->st[st->stack_end].type = st_from->st[pos].type;
    st->st[st->stack_end].size = st_from->st[pos].size;
    st->st[st->stack_end].start = st_from->st[pos].start;
    st->st[st->stack_end].value.num = st_from->st[pos].value.num;
  }
  
  st->stack_end++;

  default_st[stnum] = increase_stack(st);

}



void set_argument(int stnum, int argnum, data_buffer **text, int start, int len)
{
  stack *st0;
  int pos;
  int i;
  
  
  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st0 = default_st[stnum];
  pos = st_status->start[stnum] + argnum;
  
  
  /* fill up the 0 stack to the top if necessary */
  for(i = st0->stack_end; i <= pos; i++)
  {
    copy_stack(const_var_stack, empty_string_on_stack, 0);
  }
  
  if(st0->st[pos].type == str_sds)
  {
    sdsfree(st0->st[pos].value.str);
  }
  
  st0->st[pos].type = buffer_pointer;
  st0->st[pos].size = len;
  st0->st[pos].start = start;
  st0->st[pos].value.buf_p = text;
  
  
}


int push_op(int stnum, int command, int option, int rank)
{
  op_stack *st;

  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use op stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_op_st[stnum];

  st->st[st->stack_end].command = command;
  st->st[st->stack_end].option = option;
  st->st[st->stack_end].rank = rank;
  
  st->stack_end++;
  
  default_op_st[stnum] = increase_op_stack(st);

  return(st->stack_end - 1);
}

int execute_op(int stnum, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int top;
  
  op_stack *st;
  

  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use op stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_op_st[stnum];
  top = st->stack_end - 1;

  if(top >= st_status->start_op[stnum])
  {
    command_list[st->st[top].command].command(st->st[top].option, program_counter, status, output, stop);
  
    st->stack_end--;
  }
  
  return(top);
}


void print_stack(int stnum)
{
  stack *st;
  arg_text_return ret;
  int i;
  
  if((stnum < 0) || (stnum >= number_of_default_stacks))
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to use stack: %i, using stack 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter, stnum);
    exit_code = Exit_internal; 
    stnum = 0;
  }

  st = default_st[stnum];

  for(i = 0; i < st->stack_end; i++)
  {
    ret = argument_text(stnum, i - st_status->start[stnum]);

    printf(" stack num: %i,  string = %.*s, len= %i\n", i, ret.length, ret.str_p, ret.length);
  }
}


/* instructions for the program 
 * 
 */

/* instructions for stack manipulation */

int st_nop(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  if(debug_stack)
  {
    printf("NOP at pc: %i\n", program_counter);
  }
  return(1);
}

int st_pop(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  if(default_st[st_status->active_stack]->stack_end > st_status->start[st_status->active_stack])
  {
    pop_stack(st_status->active_stack);
  }
  
  if(debug_stack)
  {
    printf("POP at pc: %i\n", program_counter);
  }
  return(1);
}


int st_pop_to(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{

  if(option >= 0)
  {
    while(default_st[st_status->active_stack]->stack_end > (st_status->start[st_status->active_stack] + option))
    {
      pop_stack(st_status->active_stack);
    }
  }
  
  if(debug_stack)
  {
    printf("POP TO %i at pc: %i\n", option, program_counter);
  }
  return(1);
}

int st_pushvar(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  copy_stack(const_var_stack, option, st_status->active_stack);

  if(debug_stack)
  {
    printf("PUSHVAR %i at pc: %i, value = %li\n", option, program_counter, default_st[st_status->active_stack]->st[default_st[st_status->active_stack]->stack_end - 1].value.num);
  }
  return(1);
}

int st_dup(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  if(default_st[st_status->active_stack]->stack_end > st_status->start[st_status->active_stack])
  {
    copy_stack(st_status->active_stack, default_st[st_status->active_stack]->stack_end - 1, st_status->active_stack);
  }
  
  if(debug_stack)
  {
    printf("DUP at pc: %i\n", program_counter);
  }
  return(1);
}


int st_copy(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{

  if(default_st[st_status->active_stack]->stack_end > st_status->start[st_status->active_stack])
  {
    copy_stack(st_status->active_stack, default_st[st_status->active_stack]->stack_end - 1, option);
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; stack underflow in command: copyto.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user; 
  }
  
  if(debug_stack)
  {
    printf("COPY at pc: %i\n", program_counter);
  }
  return(1);
}


int st_get_arg(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int pos;
  
  pos = st_status->start[0] + option;

  if(default_st[0]->stack_end > pos)
  {
    copy_stack(0, pos, st_status->active_stack);
  }
  else
  {
    copy_stack(const_var_stack, empty_string_on_stack, st_status->active_stack);
  }
  
  if(debug_stack)
  {
    printf("GET ARG %i at pc: %i\n", option, program_counter);
  }
  return(1);
}


int st_get_arg_num(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int pos;
  int value;

  value = pop_num(st_status->active_stack);

  if(value < 0)
  {
    value = 0;
  }

  pos = st_status->start[0] + value;

  if(default_st[0]->stack_end > pos)
  {
    copy_stack(0, pos, st_status->active_stack);
  }
  else
  {
    copy_stack(const_var_stack, empty_string_on_stack, st_status->active_stack);
  }

  if(debug_stack)
  {
    printf("GET ARG by num: %i at pc: %i\n", option, program_counter);
  }
  return(1);
}


int st_push_toarg(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  stack *st,
        *st0;
  int top,
      pos;
  int i;
  
  st = default_st[st_status->active_stack];
  top = st->stack_end - 1;

  st0 = default_st[0];
  pos = st_status->start[0] + option;
  
  
  /* fill up the 0 stack to the top if necessary */
  for(i = st0->stack_end; i <= pos; i++)
  {
    copy_stack(const_var_stack, empty_string_on_stack, 0);
  }
  
  if(st->stack_end > st_status->start[st_status->active_stack])
  {
    if(st0->st[pos].type == str_sds)
    {
      sdsfree(st0->st[pos].value.str);
    }

    if(st->st[top].type == str_sds )
    {
      st0->st[pos].type = str_sds;
      st0->st[pos].size = st->st[top].size;
      st0->st[pos].value.str = sdsdup(st->st[top].value.str);
    }
    else
    {
      st0->st[pos].type = st->st[top].type;
      st0->st[pos].size = st->st[top].size;
      st0->st[pos].value.num = st->st[top].value.num;
    }  
      
  }
   
  if(debug_stack)
  {
    printf("PUSH TO ARG option:%i from stack: %i to stack 0 position: %i at pc: %i\n", option, st_status->active_stack, pos,  program_counter);
  }
  return(1);
}


int st_setstack(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  
  if((option >=0) && (option < number_of_default_stacks))
  {
    st_status->active_stack = option;
  }
  else
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to set stack: %i, no change.\n", line_counter, current_input_file_buffer->filename, local_line_counter, option);
    exit_code = Exit_internal; 
  }

  if(debug_stack)
  {
    printf("SET STACK %c at pc: %i\n", option + 'a', program_counter);
  }
  
  return(1); 
}

int st_setbase(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1;

  value1 =  pop_num(st_status->active_stack);

  if(value1 > 0)
  {
    status->base_of_args = value1;
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; trying to set base to: %i, no change.\n", line_counter, current_input_file_buffer->filename, local_line_counter, (int) value1);
    exit_code = Exit_user;
  }

  if(debug_stack)
  {
    printf("SET BASE %i at pc: %i\n", option, program_counter);
  }

  return(1);
}

int st_base_option(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{

  if(option > 0)
  {
    status->base_of_args = option;
  }
  else
  {
    fprintf(stderr, "Internal error line: %i in file: %s line: %i; trying to set base to: %i, no change.\n", line_counter, current_input_file_buffer->filename, local_line_counter, option);
    exit_code = Exit_internal;
  }

  if(debug_stack)
  {
    printf("SET BASE X %i at pc: %i\n", option, program_counter);
  }

  return(1);
}


/* instructions for argument input handling
 * 
 */

int st_beginarg(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int active_stack,
      pos;
  
  active_stack = st_status->active_stack;
 
  if(option <= 0)
  {
    pos = option;
  }
  else
  {
    pos = option - 1 - stop->pattern_len;
  }
   
  if(debug_stack)
  {
    printf("BEGINARG start = %i on stack %i at pc: %i\n", (*output)->position + 1 + pos, active_stack, program_counter);
  }
  
  if(status->stat_arg[active_stack] == no_arg_sampling)
  {
    status->start[active_stack] = (*output)->position + 1 + pos;
    status->stat_arg[active_stack] = arg_sampling_started;
    
    if(debug_stack)
    {
      printf("sampling started\n");
    }
  }
  
  return(1);
}

int st_endarg(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int active_stack;
  int pos_end;
  char *text;
  int len;
  
  active_stack = st_status->active_stack;

  if(debug_stack)
  {
    printf("ENDARG start = %i len = %i on stack %i at pc: %i\n", status->start[active_stack], (*output)->position - status->start[active_stack] + 1 - stop->pattern_len, active_stack, program_counter);
  }

  if(status->stat_arg[active_stack] == arg_sampling_started)
  {
    if(option < 2)
    {
      pos_end = push_text(active_stack, output, status->start[active_stack], (*output)->position - status->start[active_stack] + option);

      text = &((*output)->data[status->start[active_stack]]);
      len = (*output)->position - status->start[active_stack] + option;
    }
    else
    {
      pos_end = push_text(active_stack, output, status->start[active_stack], (*output)->position - status->start[active_stack] + 1 - stop->pattern_len);

      text = &((*output)->data[status->start[active_stack]]);
      len = (*output)->position - status->start[active_stack]  + 1 - stop->pattern_len;

    }    
    status->stat_arg[active_stack] = no_arg_sampling;
    
    
    if(debug_stack)
    {
      printf(" endarg stack start = %i, put arg text: %.*s len: %i to pos: %i\n", st_status->start[active_stack], len, text, len, pos_end);
      printf("sampling stopped\n");
    }
    
    if(active_stack == 0)
    {
      /* number of arguments is only counted for the first stack */
      status->num_of_args++;
    }
  }
    
  return(1);
}


/* instructions for output use and modification
 * used for filling in arguments
 */

int st_get_from_out_opt(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  sds new;

  if(option > 0)
  {
    if(option > (*output)->position)
    {
      option = (*output)->position;
    }
    
    new = sdsnewlen(&((*output)->data[(*output)->position - option + 1]), option);
  }
  else
  {
    new = sdsnewlen(&((*output)->data[status->start[st_status->active_stack] - option]), (*output)->position + 1 + option - status->start[st_status->active_stack]);
  }
  
  push_sds(st_status->active_stack, new);

  if(debug_stack)
  {
    printf("GET from OUT string: %s, num of: %i at pc: %i\n", new, option, program_counter);
  }

  return(1);
}


int st_replace_out_opt(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  arg_text_return ret;
  int top;

  top = default_st[st_status->active_stack]->stack_end - 1;

  if(top >= st_status->start[st_status->active_stack])
  {
    ret = argument_text(st_status->active_stack, top);
    
    /* position var is 1 off compared to normal, because it points to current char */
    if(option < 0)
    {
      option = 0;
    }
    
    (*output)->position -= option - 1;
   
    if((*output)->position < 0)
    {
      (*output)->position = 0;
    }
    
    *output = putchars_buffer(ret.str_p, ret.length, *output);

    (*output)->position--;

    pop_stack(st_status->active_stack);

  }

  if(debug_stack)
  {
    printf("REPLACE in OUT with opt: %i  textlen: %i at pc: %i\n", option, ret.length, program_counter);
  }

  return(1);
}


int st_replace_out(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int value;

  // value =  pop_num(st_status->active_stack);
  value =  stop->pattern_len;
  st_replace_out_opt(value, program_counter, status, output, stop);

  return(1);
}

int st_replace_out_start(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int value;

  // printf("HELLO replace out start: %i pos:%i \n",status->start[st_status->active_stack], (*output)->position);
  
  value = (*output)->position - status->start[st_status->active_stack] + 1;

  st_replace_out_opt(value, program_counter, status, output, stop);

  status->stat_arg[st_status->active_stack] = no_arg_sampling;

  return(1);
}


/* instructions for info
 * 
 */

int st_argposition(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{

  push_num(st_status->active_stack, (*output)->position); 

  if(debug_stack)
  {
    printf("ARGPOS at pc: %i\n", program_counter);
  }
  
  return(1);
}

int st_argnumber(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{

  push_num(st_status->active_stack, status->num_of_args);

  if(debug_stack)
  {
    printf("ARGNUM at pc: %i\n", program_counter);
  }
  return(1);
}


/* instructions for macro settings overrule
 * 
 */

int st_overrule(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{

  switch(option)
  {
    case 0:
      status->overrule = macro_setting_overrule_no;
      break;
    case 1:
      status->overrule = macro_setting_overrule_recursive;
      break;
    case 2:
      status->overrule = macro_setting_overrule_not_recursive;
      break;
  }
  
  return(1);
}


/* instructions for comparing
 * numbers or strings
 */

int st_if_cmp_set(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  arg_text_return ret1,
                  ret2;
  int top,
      numofargs,
      stacksize;
  int flagprev,
      memres;



  /* Special instruction used to implement if else macro.
   * Top:     string if true
   * Top - 1: string 2
   * Top - 2: string 1, or string if compare is false
   * Top - 3: flag of previous compare
   * Top - 4: true string if flag == 1
   */


  // top = default_st[st_status->active_stack]->stack_end - 1;

  stacksize = default_st[st_status->active_stack]->stack_end - st_status->start[st_status->active_stack];

  top = stacksize - 1;

  numofargs = status->num_of_args;

  // fprintf(stderr, " num args: %i top:%i size:%i\n", numofargs, top, stacksize);

  if(numofargs > 3)
  {
    numofargs = (numofargs % 3);
    if(numofargs == 0)
    {
      numofargs = 3;
    }
  }

  // fprintf(stderr, " num args 2: %i\n", numofargs);

  // print_stack(st_status->active_stack);
  
  /* check if stack is filled enough */
  if(stacksize >= (numofargs + 2))
  {
    switch(numofargs)
    {
      case 3:
        /* this is the normal case */
        flagprev = (int) argument_num(st_status->active_stack, top - 3);
        // fprintf(stderr, " flag prev: %i\n", flagprev);

        if(flagprev == 0)
        {
          /* no previous match thus compare */
          ret1 = argument_text(st_status->active_stack, top - 2);

          ret2 = argument_text(st_status->active_stack, top - 1);

          memres = 1; /* means false */

          // fprintf(stderr, " text 1: %.*s, len: %i\n", ret1.length, ret1.str_p, ret1.length);
          // fprintf(stderr, " text 2: %.*s, len: %i\n", ret2.length, ret2.str_p, ret2.length);
          
          if((ret1.length == ret2.length) && (ret1.length >= 0))
          {
            /* lengths should be equal otherwise strings can not be equal */

            /* check if strings are equal per char */
            memres = memcmp(ret1.str_p, ret2.str_p, ret1.length);

          }

          if(memres == 0)
          {
            /* strings are equal */
            /* true string to result */
            swap_on_stack(top, top - 4, st_status->active_stack);
           // fprintf(stderr, " equal\n");

            /* pop old stuff */
            pop_stack(st_status->active_stack);
            pop_stack(st_status->active_stack);
            pop_stack(st_status->active_stack);
            pop_stack(st_status->active_stack);

            /* compare is true */
            push_num(st_status->active_stack, 1ll);

          }
          else
          {
            /* strings are not equal */
            /* pop old strings */
           // fprintf(stderr, " not equal\n");
            pop_stack(st_status->active_stack);
            pop_stack(st_status->active_stack);
            pop_stack(st_status->active_stack);
          }
        }
        else
        {
          /* a previous compare is already a match, pop new strings */
          pop_stack(st_status->active_stack);
          pop_stack(st_status->active_stack);
          pop_stack(st_status->active_stack);

        }
        break;
      case 2:
        /* this should normally not happen */
        pop_stack(st_status->active_stack);
        top--;
      case 1:
        /* the string if compare is false */
        flagprev = (int) argument_num(st_status->active_stack, top - 1);

        if(flagprev == 0)
        {
          /* no previous match, so string if false is result */
          swap_on_stack(top, top - 2, st_status->active_stack);
        }

        /* clean up */
        pop_stack(st_status->active_stack);

        break;
        /* the other cases should not happen
         * but can be at end of argument collection
         * thus no action
         */
    }
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; stack underflow in IFCMPSET.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user;
  }

  if(debug_stack)
  {
    printf("IF CMP SET for ifelse at pc: %i\n", program_counter);
  }

  return(1);
}


int st_cmp_string(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  arg_text_return ret1,
                  ret2;
  int top,
      memres;
  long long int cmpresult;

  cmpresult = 0;

  top = default_st[st_status->active_stack]->stack_end - 1;


  if(top > st_status->start[st_status->active_stack])
  {
    ret1 = argument_text(st_status->active_stack, top);

    ret2 = argument_text(st_status->active_stack, top - 1);

    if(ret1.length == ret2.length)
    {
      /* lengths should be equal otherwise strings can not be equal */

      /* check if strings are equal per char */
      memres = memcmp(ret1.str_p, ret2.str_p, ret1.length);

      if(memres == 0)
      {
        cmpresult = 1;
      }
    }

    pop_stack(st_status->active_stack);
    pop_stack(st_status->active_stack);

  }
  else
  {
    /* compare result is already false */
    if(top == st_status->start[st_status->active_stack])
    {
      /* pop only 1 */
      pop_stack(st_status->active_stack);
    }
  }

  push_num(st_status->active_stack, cmpresult);

  if(debug_stack)
  {
    printf("CMP string at pc: %i\n", program_counter);
  }

  return(1);
}

int st_cmp_num(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value;
  
  value = pop_num(st_status->active_stack);
  
  if(value == option)
  {
    push_num(st_status->active_stack, 1); 
  }
  else
  {
    push_num(st_status->active_stack, 0); 
  }
  
  if(debug_stack)
  {
    printf("CMP number at pc: %i\n", program_counter);
  }

  return(1);
}


int st_greater_num(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value;

  value = pop_num(st_status->active_stack);

  if(value > option)
  {
    push_num(st_status->active_stack, 1);
  }
  else
  {
    push_num(st_status->active_stack, 0);
  }

  if(debug_stack)
  {
    printf("GREATER than number: %i at pc: %i\n", option, program_counter);
  }

  return(1);
}

int st_greaterequal_num(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value;

  value = pop_num(st_status->active_stack);

  if(value >= option)
  {
    push_num(st_status->active_stack, 1);
  }
  else
  {
    push_num(st_status->active_stack, 0);
  }

  if(debug_stack)
  {
    printf("GREATER or EQUAL than number: %i at pc: %i\n", option, program_counter);
  }

  return(1);
}

int st_smaller_num(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value;

  value = pop_num(st_status->active_stack);

  if(value < option)
  {
    push_num(st_status->active_stack, 1);
  }
  else
  {
    push_num(st_status->active_stack, 0);
  }

  if(debug_stack)
  {
    printf("SMALLER than number: %i at pc: %i\n", option, program_counter);
  }

  return(1);
}


int st_smallerequal_num(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value;

  value = pop_num(st_status->active_stack);

  if(value <= option)
  {
    push_num(st_status->active_stack, 1);
  }
  else
  {
    push_num(st_status->active_stack, 0);
  }

  if(debug_stack)
  {
    printf("SMALLER or EQUAL than number: %i at pc: %i\n", option, program_counter);
  }
  return(1);
}

int st_compare_number(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int result,
                value1,
                value2;

  
  value2 =  pop_num(st_status->active_stack);
  
  value1 =  pop_num(st_status->active_stack);

  result = 0ll;
  
  switch(option)
  {
    case 0:
      if(value1 == value2)
      {
        result = 1ll;
      }
      break;
    case 1:
      if(value1 != value2)
      {
        result = 1ll;
      }
      break;
    case 2:
      if(value1 > value2)
      {
        result = 1ll;
      }
      break;
    case 3:
      if(value1 < value2)
      {
        result = 1ll;
      }
      break;
    case 4:
      if(value1 >= value2)
      {
        result = 1ll;
      }
      break;
    case 5:
      if(value1 <= value2)
      {
        result = 1ll;
      }
      break;
  }

      
  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf("COMPARE: %i at pc: %i\n", option, program_counter);
  }

  return(1);
}



/* conditional jump instructions
 * e.g. if else then
 */


int st_ifthen(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value;
  
  
  value =  pop_num(st_status->active_stack);
  
  if(debug_stack)
  {
    printf("IF THEN at pc: %i\n", program_counter);
  }

  if(value == 0)
  {
    return(2); 
  }
  else
  {
    return(1); 
  }

  
}


int st_if(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value;
  int command,
      pc_begin,
      level;
      
  if(debug_stack)
  {
    printf("IF else endif at pc: %i\n", program_counter);
  }
  
  pc_begin = program_counter;
  
  level = 1;
  value =  pop_num(st_status->active_stack);
  
  if(value == 0)
  {
    /* find else or endif or end */
    do
    {
      program_counter++;
      command = (program_list[program_counter]).command;

      if(command == 4)
      {
        /* another if increase the level */
        level++;
      }

      if(command == 6)
      {
        /* endif decrease the level */
        level--;
      }


    }
    while((command >= 0) && !( ((command == 5) && (level <= 1)) || ((command == 6) && (level < 1)) ) );
      
    if(command < 0) /* the program counter should be placed on the end instruction */
    {
      program_counter--;
    }

    return(program_counter - pc_begin + 1); 
  }
  else
  {
    return(1); 
  }
}

int st_else(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int command,
      pc_begin,
      level;
  
    
  pc_begin = program_counter;
  level = 1;

  if(debug_stack)
  {
    printf("ELSE at pc: %i\n", program_counter);
  }

  /* find endif or end */
  do
  {
    program_counter++;
    command = (program_list[program_counter]).command;

    if(command == 4)
      {
        /* another if increase the level */
        level++;
      }

      if(command == 6)
      {
        /* endif decrease the level */
        level--;
      }
  }
  while((command >= 0) && !((command == 6) && (level < 1)));

  if(command < 0) /* the program counter should be placed on the end instruction */
  {
    program_counter--;
  }

  
  return(program_counter - pc_begin + 1); 
}


/* mathematical instructions for integers
 * 
 */


int st_add(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1,
                value2;
  
  
  value2 =  pop_num(st_status->active_stack);

  value1 =  pop_num(st_status->active_stack);
  
  push_num(st_status->active_stack, value1 + option * value2);
  
  if(debug_stack)
  {
    printf("ADD at pc: %i value1: %lli value2: %lli \n", program_counter, value1, value2);
  }

  return(1); 
}

int st_modulo(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1,
                value2,
                result;

  
  value2 =  pop_num(st_status->active_stack);

  value1 =  pop_num(st_status->active_stack);

  if(value2 != 0ll)
  {
    result = value1 % value2;
  }
  else
  {
    result = 0;
    fprintf(stderr, "Error line: %i in file: %s line: %i; modulo by 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user;
  }

  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf("MODULO at pc: %i\n", program_counter);
  }

  return(1);
}

int st_divide(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1,
                value2,
                result;

  
  value2 =  pop_num(st_status->active_stack);

  value1 =  pop_num(st_status->active_stack);

  if(value2 != 0ll)
  {
    result = value1 / value2;
  }
  else
  {
    result = LLONG_MAX;
    fprintf(stderr, "Error line: %i in file: %s line: %i; division by 0.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user;
  }
  
  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf("DIVIDE at pc: %i\n", program_counter);
  }

  return(1);
}

int st_multiply(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1,
                value2;

  
  value2 =  pop_num(st_status->active_stack);

  value1 =  pop_num(st_status->active_stack);

  push_num(st_status->active_stack, (value1 * value2));

  if(debug_stack)
  {
    printf("MULTIPLY at pc: %i\n", program_counter);
  }

  return(1);
}

long long int intpower(long long int x, long long int exp)
{
  long long int result;
  long long int y;
  
  result = 1ll;
  
  if(exp >= 0ll)
  {
    y = exp;
  }
  else
  {
    y = -exp;
  }
  
  while(y != 0ll)
  {
    if(y & 1ll)
    {
      result *= x;
    }
    
    x *= x;
    
    y >>= 1;
  }
  
  
  if(exp < 0ll)
  {
    result = 1 / result;
  }
  
  return(result);
}

int st_power(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1,
                value2,
                result;

  
  value2 =  pop_num(st_status->active_stack);

  value1 =  pop_num(st_status->active_stack);

  result = intpower(value1, value2);
  
  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf("POWER at pc: %i\n", program_counter);
  }

  return(1);
}

int st_bit_not(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1;

  
  value1 =  pop_num(st_status->active_stack);

  push_num(st_status->active_stack, (~value1));

  if(debug_stack)
  {
    printf("BIT NOT at pc: %i\n", program_counter);
  }

  return(1);
}

int st_log_not(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int value1;

  
  value1 =  pop_num(st_status->active_stack);

  push_num(st_status->active_stack, (!value1));

  if(debug_stack)
  {
    printf("LOGICAL NOT at pc: %i\n", program_counter);
  }
  return(1);
}


int st_shift_left(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int result,
                value1,
                value2;

  
  if(option == 0)
  {
    value2 =  pop_num(st_status->active_stack);
  }
  
  value1 =  pop_num(st_status->active_stack);

  if(option == 0)
  {
    result = value1 << value2;
  }
  else
  {
    result = value1 << option;
  }
    
  
  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf("SHIFT LEFT at pc: %i\n", program_counter);
  }

  return(1);
}


int st_shift_right(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int result,
                value1,
                value2;

  
  if(option == 0)
  {
    value2 =  pop_num(st_status->active_stack);
  }
  
  value1 =  pop_num(st_status->active_stack);

  if(option == 0)
  {
    result = value1 >> value2;
  }
  else
  {
    result = value1 >> option;
  }

  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf("SHIFT RIGHT at pc: %i\n", program_counter);
  }

  return(1);
}

int st_bit_logic(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int result = 0ll,
                value1,
                value2;

  
  value2 =  pop_num(st_status->active_stack);
  
  value1 =  pop_num(st_status->active_stack);

  
  switch(option)
  {
    case 0:
      result = value1 & value2;
      break;
    case 1:
      result = value1 | value2;
      break;
    case 2:
      result = value1 ^ value2;
      break;
  }

      
  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf("BIT LOGIC %i at pc: %i\n", option, program_counter);
  }

  return(1);
}


int st_logic(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  long long int result = 0ll,
                value1,
                value2;

  
  value2 =  pop_num(st_status->active_stack);
  
  value1 =  pop_num(st_status->active_stack);

  switch(option)
  {
    case 0:
      result = value1 && value2;
      break;
    case 1:
      result = value1 || value2;
      break;
  }

      
  push_num(st_status->active_stack, result);

  if(debug_stack)
  {
    printf(" LOGIC %i at pc: %i\n", option, program_counter);
  }


  return(1);
}



/* string handling instructions
 * 
 */

int st_cat(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  arg_text_return ret2,
                  ret1;
  sds new;
  int top;
  
  top = default_st[st_status->active_stack]->stack_end - 1;
  
  
  
  if(top > st_status->start[st_status->active_stack])
  {
    ret2 = argument_text(st_status->active_stack, top);
    
    ret1 = argument_text(st_status->active_stack, top - 1);
    
    new = sdsnewlen(ret1.str_p, ret1.length);
    // printf(" new1: %s-",new);
    new = sdscatlen(new, ret2.str_p, ret2.length);
    // printf(" new2: %s-\n",new);
    
    pop_stack(st_status->active_stack);
    pop_stack(st_status->active_stack);
    
    push_sds(st_status->active_stack, new);
    
  }
  else
  {
    if(top < st_status->start[st_status->active_stack])
    {
     copy_stack(const_var_stack, empty_string_on_stack, st_status->active_stack);
    }
  }

  if(debug_stack)
  {
    printf("CAT %i at pc: %i\n", option, program_counter);
  }

  
  return(1); 
}



int st_str_multiply(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  arg_text_return ret1;
  sds string,
  new;
  int top,
      i;
  long long int val;
  
  
  new = sdsempty();
  
  top = default_st[st_status->active_stack]->stack_end - 1;

  ret1 = argument_text(st_status->active_stack, top);

  string = sdsnewlen(ret1.str_p, ret1.length);

  pop_stack(st_status->active_stack);

  val = pop_num(st_status->active_stack);

  for(i = 0; i < val; i++)
  {
    new = sdscatsds(new, string);
  }
    
  push_sds(st_status->active_stack, new);

  if(debug_stack)
  {
    printf("STR MULTIPLY %i at pc: %i\n", option, program_counter);
  }

  
  sdsfree(string);

  return(1); 
}

int st_strlen(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  arg_text_return ret;
  int top;

  top = default_st[st_status->active_stack]->stack_end - 1;


  if(top >= st_status->start[st_status->active_stack])
  {
    ret = argument_text(st_status->active_stack, top);

    pop_stack(st_status->active_stack);

    push_num(st_status->active_stack, ret.length);

  }

  if(debug_stack)
  {
    printf("STR LEN %i at pc: %i\n", option, program_counter);
  }

 return(1);
}

/* instructions for operator stack
 * 
 */

int st_op_stack_ex_if(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int rank;
  op_stack *st;
 

  rank = pop_num(st_status->active_stack);

  st = default_op_st[st_status->active_stack];

  if(((program_counter + 1) < end_program_list) && ((program_list[program_counter + 1]).command >= 0))
  {
      while((st->stack_end > st_status->start_op[st_status->active_stack]) && (
        ((option == 0) && (st->st[st->stack_end - 1].rank < rank)) ||
        ((option == 1) && (st->st[st->stack_end - 1].rank <= rank)) ||
        ((option == 2) && (st->st[st->stack_end - 1].rank > rank)) ||
        ((option == 3) && (st->st[st->stack_end - 1].rank >= rank))         
      ))
      {
        execute_op(st_status->active_stack, program_counter, status, output, stop);
      }

      push_op(st_status->active_stack, (program_list[program_counter + 1]).command, (program_list[program_counter + 1]).option, rank);
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; can not push command to operator stack because at end of program.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user;
  }

  if(debug_stack)
  {
    printf("OPerator STACK Exec If at pc: %i\n", program_counter);
  }


  return(2);
}

int st_op_stack_ex(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  op_stack *st;
 
  st = default_op_st[st_status->active_stack];

  while(st->stack_end > st_status->start_op[st_status->active_stack])
  {
    execute_op(st_status->active_stack, program_counter, status, output, stop);
  }

  if(debug_stack)
  {
    printf("OPerator STACK EXec at pc: %i\n", program_counter);
  }


  return(1);
}

int st_op_stack_ex_to(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int command_to;
  op_stack *st;
 
  st = default_op_st[st_status->active_stack];

  if((program_counter + 1) < end_program_list)
  {
    command_to = (program_list[program_counter + 1]).command;
    if(command_to >= 0)
    {
      while((st->stack_end > st_status->start_op[st_status->active_stack]) && (command_to != st->st[st->stack_end - 1].command) )
      {
        // printf("exec up to, command: %i, pos: %i\n",  st->st[st->stack_end - 1].command, st->stack_end - st_status->start_op[st_status->active_stack]);
        execute_op(st_status->active_stack, program_counter, status, output, stop);
      }
      // printf("exec up to, command: %i, pos: %i\n",  st->st[st->stack_end - 1].command, st->stack_end - st_status->start_op[st_status->active_stack]);
      execute_op(st_status->active_stack, program_counter, status, output, stop);
    }
    else
    {
      fprintf(stderr, "Error line: %i in file: %s line: %i; no command to compare with operator stack because at end of program.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
      exit_code = Exit_user;
    }
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; no command to compare with operator stack because at end of program.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user;
  }

  if(debug_stack)
  {
    printf("OPerator STACK EXec TO at pc: %i\n", program_counter);
  }


  return(2);
}

int st_op_stack_push(int option, int program_counter, status_pattern *status, data_buffer **output, arg_run *stop)
{
  int rank;
  
  rank = pop_num(st_status->active_stack);
  
  if( ((program_counter + 1) < end_program_list) && ((program_list[program_counter + 1]).command >= 0) )
  {
    push_op(st_status->active_stack, (program_list[program_counter + 1]).command, (program_list[program_counter + 1]).option, rank);
  }
  else
  {
    fprintf(stderr, "Error line: %i in file: %s line: %i; can not push command to operator stack because at end of program.\n", line_counter, current_input_file_buffer->filename, local_line_counter);
    exit_code = Exit_user;
  }

  if(debug_stack)
  {
    printf("OPerator STACK PUSH at pc: %i\n", program_counter);
  }


  return(2);
}

/* main function to execute program_list
 * 
 */

arg_run exec_program(int program_counter, int pattern_len, status_pattern *status, data_buffer **output)
{
  arg_run stop;
  int command,
      option;
  
  stop.status = arg_continu;
  // stop.replace_backup = -1; /* -1 = no replace */
  // stop.replace_text = NULL;
  stop.pattern_len = pattern_len;
  
  command = (program_list[program_counter]).command;

  if(debug)
  {
    printf(" start exec program at pc %i\n", program_counter);
  }
  
  while(command > 1)  /* command 0 and 1 mean abort and stop, < 0 end of program */  
  {
    option = (program_list[program_counter]).option;
    program_counter += command_list[command].command(option, program_counter, status, output, &stop);
    // printf(" prog: %i, opt: %i, pc: %i \n", command, option, program_counter);

    command = (program_list[program_counter]).command;
  }
  
  if(command == 0)
  {
    stop.status = arg_abort;
    if(debug_stack)
    {
      printf("ABORT\n");
    }
  }
  
  if(command == 1)
  {
    stop.status = arg_stop;
    if(debug_stack)
    {
      printf("STOP\n");
    }
  }

  return(stop);
}


void print_program(int program_counter, data_buffer **out)
{
  int command,
      option;
  sds number;
  
  command = (program_list[program_counter]).command;
  
  while(command >= 0)  /* command < 0 end of program */  
  {
    option = (program_list[program_counter]).option;
    switch(command)
    {
      case 2:
        /* string */
        *out = putchar_buffer('\"',*out);
        *out = putchars_buffer(default_st[const_var_stack]->st[option].value.str, default_st[const_var_stack]->st[option].size, *out);
        *out = putchar_buffer('\"',*out);
        break;
      case 3:
        /* number */    
        number = sdsfromlonglong(default_st[const_var_stack]->st[option].value.num);
        *out = putchars_buffer(number, sdslen(number), *out);
        sdsfree(number);
        break;
      default:
        *out = putchars_buffer((uint8_t *) command_list[command].name.n8, 8, *out);
    }
    
    *out = putchar_buffer(' ',*out);
    
    program_counter ++;
    command = (program_list[program_counter]).command;
  }
  

}
