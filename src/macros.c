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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "definesizes.h"
#include "exitcodes.h"
#include "xmalloc.h"
#include "sds.h"
#include "input.h"
#include "output.h"
#include "bitapvec.h"
#include "stack.h"
#include "processor.h"
#include "macros.h"


typedef struct
{
  int macro_pos;     /* position of macro in macro list */
  int wordlist_pos;  /* position in the wordlist */
  int vec_num;       /* the vector number of vlm if found */
  int vec_index;     /* the index in the vector if vlm found */
  wordlist *list;    /* the wordlist of the found macro */
} ret_find_macro;

typedef enum 
{
    Mode_char,
    Mode_range
} charstr_mode;


macro_def (*macro_list);

int size_macro_list,
    end_macro_list;

macro_part (*macropart_list);

int size_macropart_list,
    end_macropart_list;

    
char_range (*charstr);

int size_charstr,
    end_charstr;

/* chars used for parsing to charstr */
charstr_chars (*current_charstr_chars);
         


int defined_charstr[size_defcharstr];      /* this is a list with an index to defined charstr */     
         

short int asciitohex[256];

const char numtoascii[36] = "0123456789abcdefghijklmnopqrstuvwxyz";

void nop(data_buffer **);

const builtins internal[] =
{
  {{"nop     "}, &nop},           /* 0 */
  {{"pattern "}, &add_pattern},
  {{"append_p"}, &append_pattern},
  {{"inter_p "}, &inter_pattern},
  {{"clr_pat "}, &clear_pattern},
  {{"copy_pat"}, &copy_pattern},
  {{"push    "}, &push_macro},
  {{"define  "}, &define_macro},
  {{"pshmcall"}, &push_macrocall},
  {{"defmcall"}, &def_macrocall},
  {{"copy    "}, &define_copy},
  {{"pushcopy"}, &push_copy},
  {{"pop     "}, &pop_macro},
  {{"undefine"}, &undefine_macro},
  {{"set_info"}, &set_info},
  {{"info    "}, &info_macro},
  {{"macroset"}, &set_macroset},
  {{"divert  "}, &divert},
  {{"undivert"}, &undivert},
  {{"specialc"}, &define_specialchar},
  {{"charpat "}, &define_pattern_chars},
  {{"chararg "}, &define_arg_chars},
  {{"shell   "}, &exec_command},
  {{"tempfile"}, &tempfile},
  {{"include "}, &include_file},
  {{"sinclude"}, &include_file_silent},
  {{"ifmacro?"}, &if_macro_exists},
  {{"get_var "}, &get_var},
  {{"set_var "}, &set_var},
  {{"at_last "}, &at_last},
  {{"at_first"}, &at_first},
  {{"errprint"}, &print_error},
  {{"exit    "}, &exit_really},
  {{"strindex"}, &string_index},
  {{"num2chr "}, &num_to_char},
  {{"substr  "}, &string_substr},
  {{"strtrans"}, &string_translate},
  {{"num2str "}, &number_to_string},
  {{"program "}, &add_program},
  {{"tag_m   "}, &tag_macro_position},
  {{"part_m  "}, &define_macro_part},
  {{"tag_p   "}, &tag_pattern_position},
  {{"link_prt"}, &link_patternparts}
};


const int num_internal = sizeof(internal) / sizeof(builtins);

/* a set used for variables */
sds variables[max_number_variables];

/* the var that holds the return value of a system command */ 
int sysreturn;

void init_variable(int num)
{
  if((num > 0) && (num < max_number_variables))
  {
    if(variables[num] == NULL)
    {
      variables[num] = sdsnewlen("", 0);
    }
  }
}

void init_macros(void)
{
    
  macro_list = xmalloc(sizeof(macro_def) * init_size_macro_list);
  
  size_macro_list = init_size_macro_list;
  end_macro_list = 0;
    
}

void init_macroparts(void)
{
    
  macropart_list = xmalloc(sizeof(macro_part) * init_size_macropart_list);
  
  size_macropart_list = init_size_macropart_list;
  end_macropart_list = 1; /* start with 1 not 0 in array */
    
}


void set_arg_chars(argument_chars *new, uint8_t first, uint8_t all, uint8_t allq, uint8_t num, uint8_t firstalt)
{
  
  new->first = first;
  new->all = all;
  new->allq = allq;
  new->num = num;
  new->firstalt = firstalt;

}

charstr_chars *init_charstr_chars(uint8_t start, uint8_t end, uint8_t range, uint8_t charstr, uint8_t num, uint8_t oneormore, uint8_t zeroormore, uint8_t zeroorone, uint8_t trig)
{
  charstr_chars *new;
  
  new = xmalloc(sizeof(charstr_chars));
  
  new->start_col = start;
  new->end_col = end;
  new->range_char = range;
  new->charstr_char = charstr;
  new->number = num;
  new->oneormore = oneormore;
  new->zeroormore = zeroormore;
  new->zeroorone = zeroorone;
  new->trig = trig;
    
  return(new);
}


void set_charstr_chars(charstr_chars *new, uint8_t start, uint8_t end, uint8_t range, uint8_t charstr, uint8_t num, uint8_t oneormore, uint8_t zeroormore, uint8_t zeroorone, uint8_t trig)
{
    
  new->start_col = start;
  new->end_col = end;
  new->range_char = range;
  new->charstr_char = charstr;
  new->number = num;
  new->oneormore = oneormore;
  new->zeroormore = zeroormore;
  new->zeroorone = zeroorone;
  new->trig = trig;
    
}


void init_charstr(void)
{
  int i;
  
  charstr = xmalloc(sizeof(char_range) * init_size_charstr);
  
  size_charstr = init_size_charstr;
  end_charstr = 0;

  /* clear the defined list of chartstr */
  for(i = 0; i < size_defcharstr; i++)
  {
    defined_charstr[i] = -1;
  }
      
}

void increase_charstr(void)
{
   if(end_charstr >= size_charstr)
    {
      /* increase charstr size */
      charstr = xrealloc(charstr, sizeof(char_range) * (size_charstr + add_size_charstr));
  
      size_charstr += add_size_charstr;
    }
}

int reduce_charstr(int new)
{
  if(new < end_charstr) /* just to be sure */
  {
      end_charstr = new;
  }
  return(end_charstr);
}



void init_asciitohex(void)
{
  int i;
    
  for(i = 0; i < 256; i++)
  {
    asciitohex[i] = -256;
  }
  
  for(i = 0; i <= 9; i++)
  {
    asciitohex['0' + i] = i;
  }

  asciitohex['a'] = 10;
  asciitohex['A'] = 10;
  asciitohex['b'] = 11;
  asciitohex['B'] = 11;
  asciitohex['c'] = 12;
  asciitohex['C'] = 12;
  asciitohex['d'] = 13;
  asciitohex['D'] = 13;
  asciitohex['e'] = 14;
  asciitohex['E'] = 14;
  asciitohex['f'] = 15;
  asciitohex['F'] = 15;
 
}


uint8_t next_char(uint8_t **in, int length, int *i)
{
  uint8_t input;
  int output;
  
  input = **in;

/*  printf("\n in: %c inp: %p len: %i i: %i  pi: %p \n", input, *in, length, *i, i);
 */

  if((input == current_charstr_chars->number) && (*i < length - 2))
  {
    /* a character by number */
    /* first hex value */
    (*in)++;
    output = 16 * asciitohex[**in];
    
    /* second hex value */
    (*in)++;
    output += asciitohex[**in];

    *i += 2; /* extra 2 places further */
    
  }
  else
  {
    /* normal character */
    output = input;
  }
  
  /* check for invalid hex, the values from the table calculate to < 0 if non hex codes are used */
  if(output < 0)
  {
    output = input; /* output should be the normal character */
    /* set the pointer and value back */
    (*in) -= 2;
    *i -= 2;
  }
    
  return((uint8_t)output);
}
    
int str_to_charstr(uint8_t *in, int length)
{
  int start,
      modestart= 0,
      len = 0,
      i, j;
  charstr_mode mode;
  uint8_t inchar,
          type;
  
  mode = Mode_char;
  
  start = end_charstr;
  
  /* first entry is start */  
  (charstr[start]).data.type = charrtype_start;
  (charstr[start]).data.start = charrstart_fixed;

  end_charstr++;
  
  for(i = 0; i < length; i++)
  {
    if((*in == current_charstr_chars->start_col) && (mode == Mode_char))
    {
      /* start a range of chars */
      mode = Mode_range;        
      modestart = end_charstr;
    }
    else
    {
      if((*in == current_charstr_chars->end_col) && (mode == Mode_range))
      {
        /* end of range mode, back to normal mode */
        mode = Mode_char;
        
        /* if previously rangemode really exists, then let last charstr increment length */
        if(end_charstr > modestart)
        {
          /* default type */
          type = charrtype_charincr;
          
          /* check for special end of range modes, extra char should exist */
          if(i < (length - 1))
          {
            /* star mode */
            if(in[1] == current_charstr_chars->oneormore)
            {
              type = charrtype_oneormoreincr;
              in++; /* one further extra for next in input */
              i++;
              (charstr[start]).data.start = charrstart_variable; /* this is a variable length text */
            }
            else
            {
              /* zero or more mode */
              if(in[1] == current_charstr_chars->zeroormore)
              {
                type = charrtype_zeroormoreincr;
                in++; /* one further extra for next in input */
                i++;
                (charstr[start]).data.start = charrstart_variable; /* this is a variable length text */
              }
              else
              {
                /* one time trigger mode */
                if(in[1] == current_charstr_chars->trig)
                {
                  type = charrtype_trigincr;
                  in++; /* one further extra for next in input */
                  i++;
                }
                else
                {
                  /* zero or one mode */
                  if(in[1] == current_charstr_chars->zeroorone)
                  {
                    type = charrtype_zerooroneincr;
                    in++; /* one further extra for next in input */
                    i++;
                    (charstr[start]).data.start = charrstart_variable; /* this is a variable length text */
                  }
                }
              }
            }
          }
          
          (charstr[end_charstr - 1]).data.type = type;
          (charstr[end_charstr - 1]).data.size = 1;
        }
      }
      else
      {
        if((*in == current_charstr_chars->charstr_char) && (i < (length - 1)))
        {
          /* a special character */
          
          in++; /* one further extra for next in input */
          i++;
          
          if( defined_charstr[*in] >= 0 )   /* check that the charstr is defined */
          {
            
            /* copy charstr */
            j = defined_charstr[*in] + 1; /* skip the start entry */
            
            /* copy till end */
            while(((charstr[j]).data.type == charrtype_char) || ((charstr[j]).data.type == charrtype_charincr))
            {
              (charstr[end_charstr]).complete = (charstr[j]).complete;
              
              /* adapt the type */
              if(mode == Mode_range)
              {
                /* if first entry then increment len */
                if(end_charstr == modestart)
                {
                  len++;
                }
                
                /* in range mode all charstr in the special character are converted to range mode */
                (charstr[end_charstr]).data.type = charrtype_char;
                /* all increments to length are 0 in range mode */
                (charstr[end_charstr]).data.size = 0;
                
              }
              else
              {
                /* in normal mode increase the len depending on the copied entry */
                len += (charstr[end_charstr]).data.size;
              }
              
              j++;
              end_charstr++;
              increase_charstr();
            };   
          }
          /* else wrong input, just skip this one */
          
        }
        else
        {
          /* a normal character */
          inchar = next_char(&in, length, &i);
          
          (charstr[end_charstr]).data.start = inchar;
          (charstr[end_charstr]).data.end = inchar;
          
          if(mode == Mode_char)
          {
            /* this is a new char */
            (charstr[end_charstr]).data.type = charrtype_charincr;
            /* increment charstr = 1 */
            (charstr[end_charstr]).data.size = 1;
            end_charstr++;
            len++;
          }
          else
          {
            /* in range mode */
            
            /* if first entry then increment len */
            if(end_charstr == modestart)
            {
              len++;
            }
            
            
            
            /* check if defining a range and act accordingly */
            if((inchar == current_charstr_chars->range_char) && ((charstr[end_charstr - 1]).data.type == charrtype_char) && (i < length - 1))
            {
              /* this is a range of chars */
              in++; /* one further extra for next in input */
              i++;
              
              (charstr[end_charstr - 1]).data.end = next_char(&in, length, &i);
              /* and no new charstr */
            }
            else
            {
              /* this is an additional char */
              (charstr[end_charstr]).data.type = charrtype_char;
              /* increment charstr = 0 */
              (charstr[end_charstr]).data.size = 0;
              
              end_charstr++;
              
            }
          }
        }
      }
      
    }
    in++; /* next char in input */   
    
    /* possibly need to increase charstr */
    increase_charstr();
    
  }
  
  /* end of charstr */
  (charstr[end_charstr]).data.type = charrtype_end;
  end_charstr++;

  /* fill length of charstr in first entry */  
  (charstr[start]).data.size = len;
      
  /* possibly need to increase charstr */
  increase_charstr();

  return(start);  
}


void count_macros(int macrocounters[max_size_macro + 1])
{
  int i;
  
  for(i = 0; i < end_macro_list; i++)
  {
    macrocounters[macro_list[i].name_len]++;
  }
}
    

int find_internal(uint8_t *name, int len)
{
  builtin_name search;
  int i,
      ret = -1;
  
  /* copy name to search word */
  if(len <= 8)
  {
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
    
    while( (i < num_internal) && (ret < 0))
    {
      if(search.n64 == internal[i].name.n64)
      {
        /* found internal */
        ret = i;
      }
      i++;
    }
  }
  
  if(debug)
  {
    printf(" found internal function: %i / %i\n", ret, num_internal);
  }
  
  return(ret);
}


ret_find_macro find_existing_macro(status_bitap *vecset, uint8_t *name, int macro_len, int len)
{
  int i,
  found = -1;
  ret_find_macro ret;
  
  
  if(macro_len <= 15)
  {
    ret.list = vecset->word15;
  }
  else
  {
    ret.list = vecset->word64;
  }
  
  while((found < 0) && (ret.list != NULL))
  {
    i = 0;
    do
    {
      /* first quick check */
      if(ret.list->word_length[i] == macro_len)
      {
        if(debug)
        {
          printf(" trying to find \n");
        }
        /* slow check */
        if(memcmp(name, macro_list[ret.list->macro[i]].name, len) == 0)
        {
          found = i;
          
          if(debug)
          {
            printf("found: %i \n",found);
          }
          
        }
      }
      
      i++;
    } while((i < ret.list->num_words) && (found < 0));
    
    if(found < 0)
    {
      ret.list = ret.list->next;
    }
  } 
  
  ret.wordlist_pos = found; /* position in wordlist if macro found in wordlist this is >= 0 */
  
  
  if(found >= 0)
  {
    /* found a normal macro */
    ret.macro_pos = ret.list->macro[found];
    ret.vec_num = -1;   /* not vlm */
    ret.vec_index = -1;
  }
  else
  {
    if(vecset->patlist != NULL)
    {
      /* try to find a vlm */
      ret.vec_num = 0;
      while((found < 0) && (ret.vec_num < vecset->patlist->vec_size))
      {
        i = 0;
        do
        {
          /* first quick check */
          if(vecset->patlist->masks[ret.vec_num].masks_run_patlen[i] == macro_len)
          {
            if(debug)
            {
              printf(" trying to find \n");
            }

            /* slow check */
            if((vecset->patlist->masks[ret.vec_num].masks_run[i] >= 0) && (memcmp(name, macro_list[vecset->patlist->masks[ret.vec_num].masks_run[i]].name, len) == 0))
            {
              found = i;
              
              if(debug)
              {
                printf("found: %i \n",found);
              }
            }
          }
          
          i++;
        } while((i < vecset->patlist->masks[ret.vec_num].masks_end) && (found < 0));
        
        if(found < 0)
        {
          ret.vec_num++;
        }
      } 
      
    }
    
    if(found < 0)
    {
      ret.macro_pos = -1;
      ret.vec_num = -1;
      ret.vec_index = -1;
      
      if(debug)
      {
        printf("found nothing\n");
      }
      
    }
    else
    {
      /* found vlm */
      ret.macro_pos = vecset->patlist->masks[ret.vec_num].masks_run[found];
      ret.vec_index = found;
    }
    
  }
  
  return(ret);
}


void output_arg(int argnum, data_buffer **out)
{
  arg_text_return ret;


  ret = argument_text(0, argnum);

  if(ret.length > 0)
  {
    putchars_buffer(ret.str_p, ret.length, out);
  }

}



/* Functions to be called from stack programs
 *
 * 
 *
*/

int macro_num(uint8_t *name, int name_len, uint8_t *macroset, int macroset_len)
{
  int macro_name,
      macro_name_len,
      macro_exist = -1;
  status_bitap *vecset;
  ret_find_macro ret_macro;

  
  macro_name = str_to_charstr(name, name_len);
  
  macro_name_len = (charstr[macro_name]).data.size;
  

  if(macroset_len > 0)
  {
    vecset = find_vectorset(macroset, macroset_len);
    if(vecset == NULL)
    {
      print_warning(Exit_user, "macro set: %.*s is incorrect. Using default.\n", macroset_len, macroset);
      vecset = current_status_bitap;
    }
  }
  else
  {
    vecset = current_status_bitap;
  }

  if((macro_name_len > 0) && (macro_name_len <= 64))
  {  
    /* the length of the macro is correct */
    
    /* the macro should already exist */
    ret_macro = find_existing_macro(vecset, name, macro_name_len, name_len);
    macro_exist = ret_macro.macro_pos;
  }
  
  // fprintf(stderr, " find macro: %.*s in set %.*s num: %i\n", name_len, name, macroset_len, macroset, macro_exist);
  /* remove macro name from memory */
  reduce_charstr(macro_name);
  
  return(macro_exist);
}
  
  
sds macro_infos(int macronum, int infotype, sds returnvalue)
{
  pattern_data *arglist,
               *filllist;
  uint8_t options[5];
  
  
  if((macronum >= 0) && (macronum < end_macro_list) && ((macro_list[macronum]).name_len > 0))
  {
    switch(infotype)
    {
      case 1:
        /* name of macro */
        returnvalue = sdscpylen(returnvalue, (macro_list[macronum]).name, (macro_list[macronum]).name_len);
        break;
      case 2:
        /* definition or called macro */
        returnvalue = sdscpylen(returnvalue, (macro_list[macronum]).def, (macro_list[macronum]).def_len);
        break;
      case 3:
        /* builtin or mcall */
        if((macro_list[macronum]).mcallset != NULL)
        {
          /* mcall */
          returnvalue = sdscpylen(returnvalue, (macro_list[macronum]).mcallset->name, sdslen((macro_list[macronum]).mcallset->name));
        }
        else
        {
          /* builtin */
          returnvalue = sdscpylen(returnvalue, internal[(macro_list[macronum]).builtin].name.n8, 8);
        }
        break;
      case 4: 
        /* fill options string */
        if((macro_list[macronum]).recursive == Recursive_yes)
        {
          options[0] = 'r';
        }
        else
        {
          options[0] = 'n';
        }
        
        if((macro_list[macronum]).arg_type == Run_macro_yes)
        {
          options[1] = 'r';
        }
        else
        {
          options[1] = 'n';
        }
        
        options[2] = (macro_list[macronum]).pre_size + '0';
        
        if((macro_list[macronum]).post_size >= 0)
        {
          options[3] = (macro_list[macronum]).post_size + '0';
        }
        else
        {
          options[3] = 'S';
        }
        
        options[4] = (macro_list[macronum]).virtual_char;
        
        returnvalue = sdscpylen(returnvalue, options, 5);
        break;
      case 5:       
        /* argument pattern */
        arglist = (macro_list[macronum]).arglist;
        if(arglist != NULL)
        {
          returnvalue = sdscpylen(returnvalue, arglist->name, sdslen(arglist->name));
        }
        else
        {
          returnvalue = sdscpylen(returnvalue, "", 0);
        }
        break;
      case 6:        
        /* fill pattern */
        filllist = (macro_list[macronum]).filllist;
        if(filllist != NULL)
        {
          returnvalue = sdscpylen(returnvalue, filllist->name, sdslen(filllist->name));
        }
        else
        {
          returnvalue = sdscpylen(returnvalue, "", 0);
        }
        break;
      case 7:
        /* program */
        if((macro_list[macronum]).program >= 0)
        {
          returnvalue = sds_print_program(returnvalue, (macro_list[macronum]).program);
        }
        break;
      case 9:        
        /*  info field */
        if((macro_list[macronum]).info != NULL)
        {
          returnvalue = sdscpylen(returnvalue, (macro_list[macronum]).info, (macro_list[macronum]).info_len);
        }
        break;
      case 10:        
        /*  macro or mcall */
        if((macro_list[macronum]).mcallset != NULL)
        {
          /* mcall */
          returnvalue = sdscpylen(returnvalue, "mcall", 5);
        }
        else
        {
          /* builtin */
          returnvalue = sdscpylen(returnvalue, "macro", 5);
        }
        break;
      default:
        break;
    }
  }
  else
  {
    print_warning(Exit_user, "macro with number: %i is not defined. No information can be output.\n", macronum);
  }
  
 return(returnvalue);
}  





/* from here the macro functions
 *
 * 
 * 
 * 
 * 
 *
*/


void nop(data_buffer **out)  /* no operation function */
{

}


/* pattern macros
 *
 */

void add_or_append_pattern(pattern_append_option append)
{
  pattern_data *vec;
  
  arg_text_return pat_name,
                  pattern,
                  program_text;
  int program,
      pat_num,
      level_num;
  
  pat_name = argument_text(0, 1);  /* first arg is name of pattern */
  
  vec = find_patternvec(pat_name.str_p, pat_name.length);
  
  if(vec == NULL)
  {
    /* the pattern does not yet exist */
    vec = init_patternvectors(pat_name.str_p, pat_name.length);
  }
  
  pattern = argument_text(0, 2);  /* second arg is the pattern */
  
  pat_num = str_to_charstr(pattern.str_p, pattern.length);
  
  program_text = argument_text(0, 3);  /* third arg is the program  */
  
  level_num = argument_num(0, 4);  /* fourth arg is the level of the program  */
  
  
  program = str_to_commands(program_text.str_p, program_text.length);
  
  add_to_patternvector(vec, pat_num, program, level_num, append);

  /* remove pattern string from memory */
  reduce_charstr(pat_num);

}


void add_pattern(data_buffer **out)
{
  add_or_append_pattern(pattern_no_append);
}

void append_pattern(data_buffer **out)
{
  add_or_append_pattern(pattern_append);
}

void inter_pattern(data_buffer **out)
{
  add_or_append_pattern(pattern_end);
}


void clear_pattern(data_buffer **out)
{
  pattern_data *vec;
  arg_text_return pat_name;

  pat_name = argument_text(0, 1);  /* first arg is name of pattern */

  vec = find_patternvec(pat_name.str_p, pat_name.length);

  if(vec != NULL)
  {
    clear_patternvector(vec, 0);
  }
  else
  {
    print_warning(Exit_user, "pattern with name: %.*s does not exist. Can not clear pattern.\n", pat_name.length, pat_name.str_p);
  }

}


void copy_pattern(data_buffer **out)
{
  pattern_data *vec_from,
                 *vec_to;
  arg_text_return pat_name;

  pat_name = argument_text(0, 1);  /* first arg is name of pattern from which to copy */

  vec_from = find_patternvec(pat_name.str_p, pat_name.length);

  if(vec_from != NULL)
  {
    pat_name = argument_text(0, 2);  /* second arg is name of pattern to copy to*/

    vec_to = find_patternvec(pat_name.str_p, pat_name.length);

    if(vec_to == NULL)
    {
      /* the pattern does not yet exist */
      vec_to = init_patternvectors(pat_name.str_p, pat_name.length);
    }
    
    copy_patternvector(vec_from, vec_to);
    
  }
  else
  {
    print_warning(Exit_user, "pattern with name: %.*s does not exist. Can not perform copy of pattern.\n", pat_name.length, pat_name.str_p);
  }

}


void add_program(data_buffer **out)
{
  arg_text_return program_name,
                  program_text;
  int program;

      
  program_name = argument_text(0, 1);  /* first arg is name of program */
 
  program_text = argument_text(0, 2);  /* second arg is the program  */
  
  program = str_to_commands(program_text.str_p, program_text.length);
  
  add_to_programs(program_name.str_p, program_name.length, program);

}


void tag_pattern_position(data_buffer **out)
{
  arg_text_return pat_name,
                  tag_name;
  pattern_data *pattern;

      
  pat_name = argument_text(0, 1);  /* first arg is name of the pattern */
 
  tag_name = argument_text(0, 2);  /* second arg is the name of the tag */
  
  pattern = find_patternvec(pat_name.str_p, pat_name.length);

  if(pattern != NULL)
  {
    add_to_taglist(pattern, tag_name.str_p, tag_name.length);
  }
  else
  {
    print_warning(Exit_user, "pattern with name: %.*s does not exist. Can not perform pattern tagging.\n", pat_name.length, pat_name.str_p);
  }
  
}

/* vlm functions
 *
 */

void link_patternparts(data_buffer **out)
{
  arg_text_return from_tag,
                  to_tag;
  int num_tags,
      i,
      from_index,
      to_index;

  from_tag = argument_text(0, 1);  /* first arg is from */
 
  from_index = find_in_taglist(from_tag.str_p, from_tag.length);
  
  num_tags = current_status_pattern->num_of_args - 1;
  
  for(i = 0; i < num_tags; i++)
  {
    to_tag = argument_text(0, (2 + i));  /* second arg and later is to */
    to_index = find_in_taglist(to_tag.str_p, to_tag.length);

    link_patterns(from_index, to_index);
  }
  
}


void tag_macro_position(data_buffer **out)
{
  arg_text_return ret,
                  tag_name;
  status_bitap *vecset;

 
  tag_name = argument_text(0, 1);  /* first arg is the name of the tag */

  /* vectorset name */
  /* second argument is the optional vectorset name */
  ret = argument_text(0, 2);
  
  if(ret.length > 0)
  {
    vecset = find_or_new_vectorset(ret.str_p, ret.length);
  }
  else
  {
    vecset = current_status_bitap;
    
  }

  if(vecset->patlist == NULL) 
  {
    vecset->patlist = init_patternvectors("", 0);  /* should not be found by pattern search */
  }
  
  add_to_taglist(vecset->patlist, tag_name.str_p, tag_name.length);
  
}


void define_macro_part(data_buffer **out)
{
  arg_text_return ret;
  int macro_name,
      macro_name_len;
  uint8_t *name;
  int name_len;
  vlm_recovery_option         vlm_recov_len;
  vlm_recovery_parts          vlm_recov_parts;
  pattern_append_option       vlm_type;
  status_bitap *vecset;


  /* first argument is the name of the macro part */
  ret = argument_text(0, 1);
  
  name = ret.str_p;

  name_len = ret.length;

  macro_name = str_to_charstr(ret.str_p, ret.length);
  
  macro_name_len = (charstr[macro_name]).data.size;

  if(debug)
  {
    printf(" macro part name %.*s %i, %.*s len: %i\n", ret.length, ret.str_p, macro_name, name_len, name, macro_name_len);
  }
 
  
  /* vlm options */
  /* second argument is the optional vlm options */
  ret = argument_text(0, 2);

  vlm_recov_len = Vlm_len_long; 
  vlm_recov_parts = Vlm_parts_prev;
  vlm_type = pattern_end;
  
  /* option string should have length 2 otherwise it is not valid */
  if(ret.length == 2)
  {
    /* first char for length */
    switch(ret.str_p[0])
    {
      case 'L':
        vlm_recov_len = Vlm_len_long;
        break;
      case 'E':
        vlm_recov_len = Vlm_len_short_extended;
        break;
      case 'S':
        vlm_recov_len = Vlm_len_short;
        break;
      case 'F':
        vlm_recov_len = Vlm_len_fixed;
        break;
    }
    
    /* second char for macro start and part */
    switch(ret.str_p[1])
    {
      case 's':
        vlm_recov_parts = Vlm_parts_stop;
        vlm_type = pattern_no_append;
        break;
      case 'i':
        vlm_recov_parts = Vlm_parts_stop;
        vlm_type = pattern_end;
        break;
      case 'l':
        vlm_recov_parts = Vlm_parts_prev;
        vlm_type = pattern_end;
        break;
    }
  }
  else
  {
    if(ret.length > 0)
    {
      print_warning(Exit_user, "Vlm options (%.*s) length incorrect. Setting options to default.\n", ret.length, ret.str_p);
    }
    
  }

  
  /* vectorset name */
  /* third argument is the optional vectorset name */
  ret = argument_text(0, 3);
  
  if(ret.length > 0)
  {
    vecset = find_or_new_vectorset(ret.str_p, ret.length);
  }
  else
  {
    vecset = current_status_bitap;
  }

  
  
  if((macro_name_len > 0) && (macro_name_len <= 64))
  {  
    /* the length of the macro can fit */
    
    /* check for enough space in current list */
    if(end_macropart_list >= size_macropart_list)
    {
      if(debug)
      {
        printf("macro part list is full! %i %i\n", size_macropart_list, end_macropart_list);
      }
      /* increase the size of the list */
      macropart_list = xrealloc(macropart_list, sizeof(macro_part) * (size_macropart_list + add_size_macropart_list));
      size_macropart_list += add_size_macropart_list;
    }
    
    (macropart_list[end_macropart_list]).vlm_recov_len = vlm_recov_len;
    (macropart_list[end_macropart_list]).vlm_recov_part = vlm_recov_parts;
    (macropart_list[end_macropart_list]).name = sdsnewlen(name, name_len);
    (macropart_list[end_macropart_list]).name_len = macro_name_len;
    
    
    if(vecset->patlist == NULL) 
    {
      vecset->patlist = init_patternvectors("", 0);  /* should not be found by pattern search */
    }
    
    /* macro parts have a negative index in the macro index */
    add_to_patternvector(vecset->patlist, macro_name, -end_macropart_list, 0, vlm_type);
    
    end_macropart_list++;
  }
  else
  {
    print_warning(Exit_user, "length: %i of macro part name: %.*s is incorrect. This macro is thus not defined.\n", macro_name_len, name_len, name);
  }

  /* remove macro name from memory */
  reduce_charstr(macro_name);
  
}

/* define macros
 *
 */

/* defpush is general function to define macros and macrocalls
 * 
 * arguments on stack 0 when macro:
 * 1: name of macro
 * 2: definition or nothing
 * 3: internal function or nothing
 * 4: macro options
 * 5: pattern for collecting arguments (optional)
 * 6: pattern for filling arguments (optional)
 * 7: program to execute after argument collection (optional)
 * 8: vector set (macro set) (optional)
 * 
 * 
 * arguments on stack 0 when macrocall:
 * 1: name of macro call
 * 2: name of called macro (optional)
 * 3: vector set of the called macro (optional)
 * 4: macro options
 * 5: pattern for collecting arguments (optional)
 * 6: program to execute after argument collection (optional)
 * 7: vector set (macro set) for the macro call (optional)
 * 
 */

void defpush_macro(int defpush, int defcall)
{
  int is_vlm = 0;
  int macro_name,
      macro_name_len,
      macro_exist,
      value,
      program;
  uint8_t *name;
  int name_len;
  uint8_t *definition;
  int def_len;
  int internalfunction;
  macro_option_recursive      macro_recursive;
  vlm_recovery_option         vlm_recov_len;
  vlm_recovery_parts          vlm_recov_parts;
  pattern_append_option       vlm_type;
  run_macro arg_recursive;
  int pre_size,
      post_size;
  pattern_data  *argpattern,
                *fillpattern;
  status_bitap *vecset,
               *call_vecset;
  arg_text_return ret;
  ret_find_macro ret_macro;
  macro_def *old;
  uint8_t virtual_char;
 
  
  /* first argument is the name of the macro */
  ret = argument_text(0, 1);
  
  name = ret.str_p;

  name_len = ret.length;

  macro_name = str_to_charstr(ret.str_p, ret.length);
  
  macro_name_len = (charstr[macro_name]).data.size;

  if(debug)
  {
    printf(" macro name %.*s %i, %.*s len: %i\n", ret.length, ret.str_p, macro_name, name_len, name, macro_name_len);
    fflush(stdout);
  }
  
  /* second argument is the definition of the macro or name of the called macro*/
  ret = argument_text(0, 2);

  
  definition =  ret.str_p;
  def_len = ret.length;
  
  /* default values */
  internalfunction = 0;
  call_vecset = NULL;
  
  /* third argument is the name of the internal function or vector set of the called macro*/
  ret = argument_text(0, 3);
  
  if(defcall == 0)
  {
    if(ret.length > 0)
    {
      internalfunction = find_internal(ret.str_p, ret.length); 
      
      if(internalfunction < 0)
      {
        internalfunction = 0; /* the nop */
        
        print_warning(Exit_user, "internal function: %.*s does not exist. NOP is used instead.\n", ret.length, ret.str_p);
      }
    }
  }
  else
  {
    if(ret.length > 0)
    {
      call_vecset = find_or_new_vectorset(ret.str_p, ret.length);
    }
    else
    {
      call_vecset = current_status_bitap;

    }
  }
  
  /* fourth argument for options of the macro */
  ret = argument_text(0, 4);
  
  /* default values */
  macro_recursive = Recursive_no;
  arg_recursive = Run_macro_no;
  pre_size = 0;
  post_size = 0;
  virtual_char = 255;
  
  /* option string should have length 4 or 5 otherwise it is not valid */
  if((ret.length == 4) || (ret.length == 5))
  {
    /* first char for macro recursive */
    if(ret.str_p[0] == 'r')
    {
      macro_recursive = Recursive_yes;
    }
    
    /* second char for argument recursive */
    if(ret.str_p[1] == 'r')
    {
      arg_recursive = Run_macro_yes;
    }
    
    /* third char for pre size of macro name */
    value = asciitohex[ret.str_p[2]];
    if(value >= 0)
    {
      pre_size = value;
    }
    
    /* fourth char for post size of macro name */
    value = asciitohex[ret.str_p[3]];
    if(value >= 0)
    {
      post_size = value;
    }
    else
    {
      /* check if the post size is the start of a macro */
      if(ret.str_p[3] == 'S')
      {
        post_size = -1;
      }
    }
    
    /* optional fifth char for use as virtual char */
    if(ret.length == 5)
    {
      virtual_char = ret.str_p[4];
    }
    
    if(debug)
    {
      printf("valid macro options %i %i %i %i -%c-\n", macro_recursive, arg_recursive, pre_size, post_size, virtual_char);
      fflush(stdout);
    }
    
  }
  else
  {
    if(ret.length > 0)
    {
      print_warning(Exit_user, "Macro options (%.*s) length incorrect. Setting options to default.\n", ret.length, ret.str_p);
    }
  }
  
  /* fifth argument sets the argument pattern to be used */
  ret = argument_text(0, 5);
  
  if(ret.length > 0)
  {
    argpattern = find_patternvec(ret.str_p, ret.length);

    if(argpattern == NULL) 
    {
      print_warning(Exit_user, "pattern: %.*s in macro definition is incorrect. Not using an argument collection pattern.\n", ret.length, ret.str_p);
    }
  }
  else
  {
    argpattern = NULL;
  }

   
  
  if(debug)
  {
    printf(" argpattern pointer: %p\n", argpattern);
    fflush(stdout);
  }
  
  /* fillpattern */
  if(defcall == 0)
  {
    /* sixth argument sets the fill pattern to be used */
    ret = argument_text(0, 6);
    
    if(ret.length > 0)
    {
      fillpattern = find_patternvec(ret.str_p, ret.length);
      
      if(fillpattern == NULL) 
      {
        print_warning(Exit_user, "pattern: %.*s in macro definition is incorrect. Not using an argument substitution pattern.\n", ret.length, ret.str_p);
      }
      
    }
    else
    {
      fillpattern = NULL;
    }
  }
  else
  {
    fillpattern = NULL;
  }    
  
  if(debug)
  {
    printf(" fillpattern pointer: %p\n", fillpattern);
    fflush(stdout);
  }

  
  /* program after argument collection */ 
  if(defcall == 0)
  {
    /* seventh argument is the optional program to be executed after argument collection */
    ret = argument_text(0, 7);
  }
  else
  {
    /* sixth argument is the optional program to be executed after argument collection */
    ret = argument_text(0, 6);
  }    
  
  if(ret.str_p != NULL)
  {
    program = str_to_commands(ret.str_p, ret.length);
  }
  else
  {
    program = -1;
  }
  
  /* vectorset name */
  if(defcall == 0)
  {
    /* eighth argument is the optional vectorset name */
    ret = argument_text(0, 8);
  }
  else
  {
    /* seventh argument is the optional vectorset name */
    ret = argument_text(0, 7);
  }
  
  if(ret.length > 0)
  {
    vecset = find_or_new_vectorset(ret.str_p, ret.length);
  }
  else
  {
    vecset = current_status_bitap;
    
  }

  
  
  /* vlm options */
  if(defcall == 0)
  {
    /* ninth argument is the optional vlm options */
    ret = argument_text(0, 9);
  }
  else
  {
    /* eighth argument is the optional vlm options */
    ret = argument_text(0, 8);
  }

  vlm_recov_len = Vlm_len_short_extended; 
  vlm_recov_parts = Vlm_parts_stop;
  vlm_type = pattern_no_append;
  
  /* option string should have length 2 otherwise it is not valid */
  if(ret.length == 2)
  {
    /* if vlm option string exists, then always vlm */
    is_vlm = 1;
    
    /* first char for length */
    switch(ret.str_p[0])
    {
      case 'L':
        vlm_recov_len = Vlm_len_long;
        break;
      case 'E':
        vlm_recov_len = Vlm_len_short_extended;
        break;
      case 'S':
        vlm_recov_len = Vlm_len_short;
        break;
      case 'F':
        vlm_recov_len = Vlm_len_fixed;
        break;
    }
    
    /* second char for macro start and part */
    switch(ret.str_p[1])
    {
      case 'n':
        vlm_recov_parts = Vlm_parts_stop;
        vlm_type = pattern_no_append;
        break;
      case 'e':
        vlm_recov_parts = Vlm_parts_stop;
        vlm_type = pattern_end;
        break;
      case 'l':
        vlm_recov_parts = Vlm_parts_prev;
        vlm_type = pattern_end;
        break;
    }
  }
  else
  {
    if(ret.length > 0)
    {
      print_warning(Exit_user, "Vlm options (%.*s) length incorrect. Setting options to default.\n", ret.length, ret.str_p);
    }
    
  }

  
  if((macro_name_len > 0) && (macro_name_len <= 64))
  {  
    /* the length of the macro can fit */

    if(debug)
    {
      printf("\n testing if macro already exists\n");
      fflush(stdout);
    }
    
    /* does the macro already exist? */
    ret_macro = find_existing_macro(vecset, name, macro_name_len, name_len);
    macro_exist = ret_macro.macro_pos;
    
    if(debug)
    {
      printf("\n macro already exists place=%i\n", macro_exist);
      fflush(stdout);
    }
    
    if(macro_exist >= 0)
    {
      /* macro already exists, thus only adapt macro */
      
      if(defpush == 0)
      {
        /* reserve space for current macro definition and copy */
        old = xmalloc(sizeof(macro_def));
        memcpy(old, &(macro_list[macro_exist]), sizeof(macro_def));
        (macro_list[macro_exist]).prev = old;
      }
      
      /* fill in new macro data and definition */
      (macro_list[macro_exist]).recursive = macro_recursive;
      (macro_list[macro_exist]).vlm_recov_len = vlm_recov_len;
      (macro_list[macro_exist]).vlm_recov_part = vlm_recov_parts;
      (macro_list[macro_exist]).arg_type = arg_recursive;
      if(def_len >= 0)
      {
        (macro_list[macro_exist]).def = sdsnewlen(definition, def_len);
      }
      else
      {
        (macro_list[macro_exist]).def = sdsnewlen(definition, 0);
      }
      (macro_list[macro_exist]).def_len = def_len;
      (macro_list[macro_exist]).builtin = internalfunction;
      (macro_list[macro_exist]).pre_size = pre_size;
      (macro_list[macro_exist]).post_size = post_size;
      (macro_list[macro_exist]).arglist = argpattern;
      (macro_list[macro_exist]).filllist = fillpattern;
      (macro_list[macro_exist]).program = program;
      (macro_list[macro_exist]).virtual_char = virtual_char;
      (macro_list[macro_exist]).mcallset = call_vecset;

      (macro_list[macro_exist]).info = NULL;
      (macro_list[macro_exist]).info_len = -1;
      
     if(debug)
     {
       printf("\n change macro:%s def:%s: place=%i\n", (macro_list[macro_exist]).name, (macro_list[macro_exist]).def, macro_exist);
       fflush(stdout);
     }
      
    }
    else
    {
      /* create new macro and entries in vector lists */
      
      /* check for enough space in current list */
      if(end_macro_list >= size_macro_list)
      {
        if(debug)
        {
          printf("macro list is full! %i %i\n", size_macro_list, end_macro_list);
        }
        /* increase the size of the list */
        macro_list = xrealloc(macro_list, sizeof(macro_def) * (size_macro_list + add_size_macro_list));
        size_macro_list += add_size_macro_list;
      }
      
      (macro_list[end_macro_list]).recursive = macro_recursive;
      (macro_list[end_macro_list]).vlm_recov_len = vlm_recov_len;
      (macro_list[end_macro_list]).vlm_recov_part = vlm_recov_parts;
      (macro_list[end_macro_list]).arg_type = arg_recursive;
      (macro_list[end_macro_list]).name = sdsnewlen(name, name_len);
      (macro_list[end_macro_list]).name_len = macro_name_len;
      if(def_len >= 0)
      {
        (macro_list[end_macro_list]).def = sdsnewlen(definition, def_len);
      }
      else
      {
        (macro_list[end_macro_list]).def = sdsnewlen(definition, 0);
      }
      (macro_list[end_macro_list]).def_len = def_len;
      (macro_list[end_macro_list]).builtin = internalfunction;
      (macro_list[end_macro_list]).pre_size = pre_size;
      (macro_list[end_macro_list]).post_size = post_size;
      (macro_list[end_macro_list]).prev = NULL;
      (macro_list[end_macro_list]).arglist = argpattern;
      (macro_list[end_macro_list]).filllist = fillpattern;
      (macro_list[end_macro_list]).program = program;
      (macro_list[end_macro_list]).virtual_char = virtual_char;
      (macro_list[end_macro_list]).mcallset = call_vecset;

      (macro_list[end_macro_list]).info = NULL;
      (macro_list[end_macro_list]).info_len = -1;
      
      if(debug)
      {
        printf("\n add macro:%s def:%s: place=%i\n", (macro_list[end_macro_list]).name, (macro_list[end_macro_list]).def, end_macro_list);
        fflush(stdout);
     }

      if(stepdebug)
      {
        printf("New macro defined:\n");
        printf(" name=%s, len=%i\n", (macro_list[end_macro_list]).name, (macro_list[end_macro_list]).name_len);
        printf(" definition=%.*s, len=%i\n", (macro_list[end_macro_list]).def_len, (macro_list[end_macro_list]).def, (macro_list[end_macro_list]).def_len);
        printf(" internal function=%.*s\n", 8, internal[(macro_list[end_macro_list]).builtin].name.n8);         
        printf(" vlm recovery length=%i\n", (macro_list[end_macro_list]).vlm_recov_len);
        printf(" recursive macro=%i, arguments=%i\n", (macro_list[end_macro_list]).recursive, (macro_list[end_macro_list]).arg_type);
        printf(" size pre=%i, post=%i\n", (macro_list[end_macro_list]).pre_size, (macro_list[end_macro_list]).post_size);
        fflush(stdout);
     }

      if(((charstr[macro_name]).data.start == charrstart_fixed) && (is_vlm == 0))
      {
        /* fixed length macro */
        
        add_to_vectors2(vecset->vec, macro_name);
      
        if(macro_name_len <= 15)
        {
          add_to_wordlist2(vecset->word15, macro_name, end_macro_list);
      
        }
        else
        {
          add_to_wordlist2(vecset->word64, macro_name, end_macro_list);
      
        }
      }
      else
      {
        /* variable length macro */
        if(debug)
        {
          printf("\n vlm:%s \n", (macro_list[end_macro_list]).name);
        }

        if(vecset->patlist == NULL) 
        {
          vecset->patlist = init_patternvectors("", 0);  /* should not be found by pattern search */
        }
        
        add_to_patternvector(vecset->patlist, macro_name, end_macro_list, 0, vlm_type);

      }
      
      end_macro_list++;
      
    }
  }
  else
  {
    print_warning(Exit_user, "length: %i of macro name: %.*s is incorrect. This macro is thus not defined.\n", macro_name_len, name_len, name);
  }

  /* remove macro name from memory */
  reduce_charstr(macro_name);
}


void push_macro(data_buffer **out)
{

  defpush_macro(0, 0);

}

void define_macro(data_buffer **out)
{

  defpush_macro(1, 0);

}

void def_macrocall(data_buffer **out)
{
  
  defpush_macro(1, 1);

}

void push_macrocall(data_buffer **out)
{
  
  defpush_macro(0, 1);

}



/* copy_macro is general function to copy macros and macrocalls
 * 
 * arguments on stack 0 when macro:
 * 1: name of source macro
 * 2: name of destination macro
 * 3: vector set (macro set) of source (optional)
 * 4: vector set (macro set) of destination (optional)
 * 
 */

void copy_macro(int defpush)
{
  int macro_dest_name,
      macro_dest_name_len,
      macro_source_name,
      macro_source_name_len,
      macro_exist,
      macro_source;
  uint8_t *name_source;
  int name_source_len;
  uint8_t *name_dest;
  int name_dest_len;
  status_bitap *source_vecset,
               *dest_vecset;
  macro_def *old;
  arg_text_return ret;
  ret_find_macro ret_macro;
 
  
  /* first argument is the name of the source macro */
  ret = argument_text(0, 1);
  
  name_source = ret.str_p;
  
  name_source_len = ret.length;
  
  macro_source_name = str_to_charstr(ret.str_p, ret.length);
  
  macro_source_name_len = (charstr[macro_source_name]).data.size;
  
  
  /* second argument is the name of the destination macro */
  ret = argument_text(0, 2);
  
  name_dest = ret.str_p;
  
  name_dest_len = ret.length;
  
  macro_dest_name = str_to_charstr(ret.str_p, ret.length);
  
  macro_dest_name_len = (charstr[macro_dest_name]).data.size;
  
  // fprintf(stderr, "copy macro from %.*s to %.*s.\n", name_source_len, name_source, name_dest_len, name_dest);
    
  
  /* vectorset name */
  /* third argument is the optional vectorset name */
  ret = argument_text(0, 3);
  
  if(ret.length > 0)
  {
    source_vecset = find_vectorset(ret.str_p, ret.length);
    if(source_vecset == NULL)
    {
      print_warning(Exit_user, "macro set: %.*s is incorrect. Using default.\n", ret.length, ret.str_p);
      source_vecset = current_status_bitap;
    }
  }
  else
  {
    source_vecset = current_status_bitap;
  }
  
  /* fourth argument is the optional vectorset name */
  ret = argument_text(0, 4);
  
  if(ret.length > 0)
  {
    dest_vecset = find_or_new_vectorset(ret.str_p, ret.length);
  }
  else
  {
    dest_vecset = current_status_bitap;
  }
  
  ret_macro = find_existing_macro(source_vecset, name_source, macro_source_name_len, name_source_len);
  macro_source = ret_macro.macro_pos;
  
  
  if(macro_source >= 0)
  {
    /* source macro exists */
    
    if((macro_dest_name_len > 0) && (macro_dest_name_len <= 64))
    {  
      /* the length of the macro can fit */
      
      if(debug)
      {
        printf("\n testing if macro already exists\n");
      }
      
      /* does the macro already exist? */
      ret_macro = find_existing_macro(dest_vecset, name_dest, macro_dest_name_len, name_dest_len);
      macro_exist = ret_macro.macro_pos;
      
      if(debug)
      {
        printf("\n macro already exists place=%i\n", macro_exist);
      }
      
      if(macro_exist >= 0)
      {
        /* macro already exists, thus only adapt macro */
        
        if(defpush == 0)
        {
          /* reserve space for current macro definition and copy */
          old = xmalloc(sizeof(macro_def));
          memcpy(old, &(macro_list[macro_exist]), sizeof(macro_def));
        }
        else
        {
          old = (macro_list[macro_exist]).prev;
        }
        
        
        /* copy macro */
        memcpy(&(macro_list[macro_exist]), &(macro_list[macro_source]), sizeof(macro_def));
        /* copy sds strings */
        if((macro_list[macro_exist]).def_len >= 0)
        {
          (macro_list[macro_exist]).def = sdsnewlen((macro_list[macro_exist]).def, (macro_list[macro_exist]).def_len);
        }
        else
        {
          (macro_list[macro_exist]).def = sdsnewlen((macro_list[macro_exist]).def, 0);
        }
        /* set to previous macro */
        (macro_list[macro_exist]).prev = old;
        
        
        if(debug)
        {
          printf("\n change macro:%s def:%s: place=%i\n", (macro_list[macro_exist]).name, (macro_list[macro_exist]).def, macro_exist);
        }
        
      }
      else
      {
        /* create new macro and entries in vector lists */
        
        /* check for enough space in current list */
        if(end_macro_list >= size_macro_list)
        {
          if(debug)
          {
            printf("macro list is full! %i %i\n", size_macro_list, end_macro_list);
          }
          /* increase the size of the list */
          macro_list = xrealloc(macro_list, sizeof(macro_def) * (size_macro_list + add_size_macro_list));
          size_macro_list += add_size_macro_list;
        }
        
        /* copy macro */
        memcpy(&(macro_list[end_macro_list]), &(macro_list[macro_source]), sizeof(macro_def));
        /* new name */
        (macro_list[end_macro_list]).name = sdsnewlen(name_dest, name_dest_len);
        (macro_list[end_macro_list]).name_len = macro_dest_name_len;
        /* copy sds strings */
        if((macro_list[end_macro_list]).def_len >= 0)
        {
          (macro_list[end_macro_list]).def = sdsnewlen((macro_list[end_macro_list]).def, (macro_list[end_macro_list]).def_len);
        }
        else
        {
          (macro_list[end_macro_list]).def = sdsnewlen((macro_list[end_macro_list]).def, 0);
        }
        /* reset previous macro */
        (macro_list[end_macro_list]).prev = NULL;
        
        if(debug)
        {
          printf("\n add macro:%s def:%s: place=%i\n", (macro_list[end_macro_list]).name, (macro_list[end_macro_list]).def, end_macro_list);
        }
        
                
        /* add to vector list */
        if((charstr[macro_dest_name]).data.start == charrstart_fixed)
        {
          /* fixed length macro */
          
          add_to_vectors2(dest_vecset->vec, macro_dest_name);
          
          if(macro_dest_name_len <= 15)
          {
            add_to_wordlist2(dest_vecset->word15, macro_dest_name, end_macro_list);
            
          }
          else
          {
            add_to_wordlist2(dest_vecset->word64, macro_dest_name, end_macro_list);
            
          }
        }
        else
        {
          /* variable length macro */
          if(debug)
          {
            printf("\n vlm:%s \n", (macro_list[end_macro_list]).name);
          }
          
          if(dest_vecset->patlist == NULL) 
          {
            dest_vecset->patlist = init_patternvectors("", 0);  /* should not be found by pattern search */
          }
          
          add_to_patternvector(dest_vecset->patlist, macro_dest_name, end_macro_list, 0, pattern_no_append);
          
        }
        
        end_macro_list++;
        
      }
    }
    else
    {
      print_warning(Exit_user, "length: %i of macro name: %.*s is incorrect. This macro is thus not defined.\n", macro_dest_name_len, name_dest_len, name_dest);
      // exit_code = Exit_user; 
    }
  }
  else
  {
    /* source macro does not exist */
    print_warning(Exit_user, "macro name: %.*s for copying does not exist.\n", name_source_len, name_source);
    // exit_code = Exit_user; 
  }
  
  /* remove macro names from memory */
  reduce_charstr(macro_source_name);
}

void push_copy(data_buffer **out)
{

  copy_macro(0);

}

void define_copy(data_buffer **out)
{

  copy_macro(1);

}



void undefpop_macro(int undefpop)
{
  int macro_name,
  macro_name_len,
  macro_exist;
  uint8_t *name;
  status_bitap *vecset;
  int name_len;
  ret_find_macro ret_macro;
  arg_text_return ret;
  macro_def *prev,
            *now;
  
  
  /* first argument is the name of the macro */
  ret = argument_text(0, 1);
  
  name = ret.str_p;
  
  name_len = ret.length;
  
  macro_name = str_to_charstr(ret.str_p, ret.length);
  
  macro_name_len = (charstr[macro_name]).data.size;
  
  if(debug)
  {
    printf(" undefine macro name %.*s %i, %.*s len: %i\n", ret.length, ret.str_p, macro_name, name_len, name, macro_name_len);
  }

  /* second argument is the optional vector set */
  ret = argument_text(0, 2);

  if(ret.length > 0)
  {
    vecset = find_vectorset(ret.str_p, ret.length);
    if(vecset == NULL)
    {
      print_warning(Exit_user, "macro set: %.*s is incorrect. Using default.\n", ret.length, ret.str_p);
      vecset = current_status_bitap;
    }
  }
  else
  {
    vecset = current_status_bitap;
  }

  
  if((macro_name_len > 0) && (macro_name_len <= 64))
  {  
    /* the length of the macro is correct */
    
    /* the macro should already exist */
    ret_macro = find_existing_macro(vecset, name, macro_name_len, name_len);
    macro_exist = ret_macro.macro_pos;
    
    if(debug)
    {
      printf(" find existing macro: %i list pos: %i list p:%p\n", ret_macro.macro_pos, ret_macro.wordlist_pos, ret_macro.list);
    }
    
    if(macro_exist >= 0)
    {
      /* macro exists */
      if((undefpop == 0) && (macro_list[macro_exist].prev != NULL))
      {
        /* pop*/
        prev = (macro_list[macro_exist]).prev;
        sdsfree((macro_list[macro_exist]).def);
        if((macro_list[macro_exist]).info != NULL)
        {
          sdsfree((macro_list[macro_exist]).info);
        }
        memcpy(&(macro_list[macro_exist]), prev, sizeof(macro_def));
        xfree(prev);
      }
      else
      {
        /* undefine */
        prev = (macro_list[macro_exist]).prev;
        
        /* delete macro stack definitions */
        while(prev != NULL)
        {
          now = prev;
          prev = prev->prev;
          sdsfree(now->def);
          if(now->info != NULL)
          {
            sdsfree(now->info);
          }
          xfree(now);
        }
        
        /* delete current entry in macro list */
        sdsfree((macro_list[macro_exist]).name);
        (macro_list[macro_exist]).name_len = 0;
        sdsfree((macro_list[macro_exist]).def);
        if((macro_list[macro_exist]).info != NULL)
        {
          sdsfree((macro_list[macro_exist]).info);
        }
        
        if(ret_macro.wordlist_pos >= 0)
        {
          /* normal macro */
          delete_from_wordlist(ret_macro.list, ret_macro.wordlist_pos);
        }
        else
        {
          /* vlm */
          delete_from_pattern(vecset->patlist, ret_macro.vec_num, ret_macro.vec_index); 
        }
        
      }
    }
    else
    {
      print_warning(Exit_user, "macro: %.*s does not exist.\n", name_len, name);
    }
  }
  else
  {
    print_warning(Exit_user, "length: %i of macro name: %.*s is incorrect. This macro name is not used for undefine or pop.\n", macro_name_len, name_len, name);
  }
  
  /* remove macro name from memory */
  reduce_charstr(macro_name);
}


void pop_macro(data_buffer **out)
{

  undefpop_macro(0);

}

void undefine_macro(data_buffer **out)
{

  undefpop_macro(1);

}


void set_info(data_buffer **out)
{
  int macro_name,
      macro_name_len,
      macro_exist,
      name_len;
  ret_find_macro ret_macro;
  arg_text_return ret,
                  macroset,
                  info;
  status_bitap *vecset;
  uint8_t *name;

  /* first argument is the name of the macro */
  ret = argument_text(0, 1);
  
  name = ret.str_p;
  
  name_len = ret.length;
  
  macro_name = str_to_charstr(ret.str_p, ret.length);
  
  macro_name_len = (charstr[macro_name]).data.size;

  /* second argument is the info */
  info = argument_text(0, 2);
 
  /* third argument is the optional macro set */
  macroset = argument_text(0, 3);

  if(macroset.length > 0)
  {
    vecset = find_vectorset(macroset.str_p, macroset.length);
    if(vecset == NULL)
    {
      print_warning(Exit_user, "macro set: %.*s is incorrect. Using default.\n", ret.length, ret.str_p);
      vecset = current_status_bitap;
    }
  }
  else
  {
    vecset = current_status_bitap;
  }

  
  
  if(debug)
  {
    printf(" set info of macro %.*s %i, %.*s len: %i\n", ret.length, ret.str_p, macro_name, name_len, name, macro_name_len);
  }
    
  if((macro_name_len > 0) && (macro_name_len <= 64))
  {  
    /* the length of the macro is correct */
    
    /* the macro should already exist */
    ret_macro = find_existing_macro(vecset, name, macro_name_len, name_len);
    macro_exist = ret_macro.macro_pos;
    
    if(macro_exist >= 0)
    {
      if((macro_list[macro_exist]).info != NULL)
      {
        (macro_list[macro_exist]).info = sdscpylen((macro_list[macro_exist]).info, info.str_p, info.length);
      }
      else
      {
        (macro_list[macro_exist]).info = sdsnewlen(info.str_p, info.length);
      }
      
      (macro_list[macro_exist]).info_len = info.length;
    }
    else
    {
      print_warning(Exit_user, "macro name: %.*s is incorrect. No information can be set.\n", name_len, name);
    }
    
  }
  else
  {
      print_warning(Exit_user, "length: %i of macro name: %.*s is incorrect. No information can be set.\n", macro_name_len, name_len, name);
  }
  
  /* remove macro name from memory */
  reduce_charstr(macro_name);
  
}




void info_macro(data_buffer **out)
{
  int macro_name,
      macro_name_len,
      macro_exist,
      start,
      len,
      name_len;
  ret_find_macro ret_macro;
  arg_text_return ret,
                  macroset;
  pattern_data *arglist,
                 *filllist;
  status_bitap *vecset;
  uint8_t *name;
  uint8_t options[5];

  /* first argument is the name of the macro */
  ret = argument_text(0, 1);
  
  name = ret.str_p;
  
  name_len = ret.length;
  
  macro_name = str_to_charstr(ret.str_p, ret.length);
  
  macro_name_len = (charstr[macro_name]).data.size;
  
  /* second argument is the optional macro set */
  macroset = argument_text(0, 2);

  if(macroset.length > 0)
  {
    vecset = find_vectorset(macroset.str_p, macroset.length);
    if(vecset == NULL)
    {
      print_warning(Exit_user, "macro set: %.*s is incorrect. Using default.\n", ret.length, ret.str_p);
      vecset = current_status_bitap;
    }
  }
  else
  {
    vecset = current_status_bitap;
  }
  
  if(debug)
  {
    printf(" info of macro name %.*s %i, %.*s len: %i\n", ret.length, ret.str_p, macro_name, name_len, name, macro_name_len);
  }
    
  if((macro_name_len > 0) && (macro_name_len <= 64))
  {  
    /* the length of the macro is correct */
    
    /* the macro should already exist */
    ret_macro = find_existing_macro(vecset, name, macro_name_len, name_len);
    macro_exist = ret_macro.macro_pos;
    
    if(macro_exist >= 0)
    {
      /* arg 3: builtin or called macro set */
      start = (*out)->position;
      
      if((macro_list[macro_exist]).mcallset != NULL)
      {
        /* mcall */
        len = sdslen((macro_list[macro_exist]).mcallset->name);
        putchars_buffer((macro_list[macro_exist]).mcallset->name, len, out);
      }
      else
      {
        /* builtin */
        len = 8;
        putchars_buffer((uint8_t *) internal[(macro_list[macro_exist]).builtin].name.n8, 8, out);
      }
      
      set_argument(0, 3, out, start, len);
      
      
      /* fill options string */
      if((macro_list[macro_exist]).recursive == Recursive_yes)
      {
        options[0] = 'r';
      }
      else
      {
        options[0] = 'n';
      }
      
      if((macro_list[macro_exist]).arg_type == Run_macro_yes)
      {
        options[1] = 'r';
      }
      else
      {
        options[1] = 'n';
      }
      
      options[2] = (macro_list[macro_exist]).pre_size + '0';

      if((macro_list[macro_exist]).post_size >= 0)
      {
        options[3] = (macro_list[macro_exist]).post_size + '0';
      }
      else
      {
        options[3] = 'S';
      }
      
      options[4] = (macro_list[macro_exist]).virtual_char;
      
      /* arg 4: options */
      start = (*out)->position;
      putchars_buffer(options, 5, out);
      set_argument(0, 4, out, start, 5);

      /* arg 5: argument pattern */
      arglist = (macro_list[macro_exist]).arglist;
      start = (*out)->position;
      if(arglist != NULL)
      {
        len = sdslen(arglist->name);
        putchars_buffer( arglist->name, len, out);
      }
      else
      {
        len = 0;
      }
      set_argument(0, 5, out, start, len);

      /* arg 6: fill pattern */
      filllist = (macro_list[macro_exist]).filllist;
      start = (*out)->position;
      if(filllist != NULL)
      {
        len = sdslen(filllist->name);
        putchars_buffer( filllist->name, len, out);
      }
      else
      {
        len = 0;
      }
      set_argument(0, 6, out, start, len);

      /* arg 7: program */
      start = (*out)->position;
      if((macro_list[macro_exist]).program >= 0)
      {
        print_program((macro_list[macro_exist]).program, out);
      }
      set_argument(0, 7, out, start, (*out)->position - start);

      /* arg 8: macro set */
      start = (*out)->position;
      len = sdslen(vecset->name);
      putchars_buffer(vecset->name, len, out);
      set_argument(0, 8, out, start, len);

      /* arg 9: macro or mcall */
      start = (*out)->position;
      if((macro_list[macro_exist]).info != NULL)
      {
        putchars_buffer((macro_list[macro_exist]).info, (macro_list[macro_exist]).info_len, out);

        set_argument(0, 9, out, start, (macro_list[macro_exist]).info_len);
      }
      else
      {
        if((macro_list[macro_exist]).mcallset != NULL)
        {
          /* mcall */
          putchars_buffer("mcall", 5, out);
        }
        else
        {
          /* builtin */
          putchars_buffer("macro", 5, out);
        }
        
        set_argument(0, 9, out, start, 5);
      }
      
      
      /* arg 2: definition or called macro */
      /* output of macro is the definition */
      (*out)->start = (*out)->position;
      /* on the stack the definition without quotes */
      start = (*out)->position;
      putchars_buffer((macro_list[macro_exist]).def, (macro_list[macro_exist]).def_len, out);
      set_argument(0, 2, out, start, (macro_list[macro_exist]).def_len);
      
    }
    else
    {
      print_warning(Exit_user, "macro name: %.*s is incorrect. No information can be output.\n", name_len, name);
    }
    
  }
  else
  {
      print_warning(Exit_user, "length: %i macro name: %.*s is incorrect. No information can be output.\n", macro_name_len, name_len, name);
  }
  
  /* remove macro name from memory */
  reduce_charstr(macro_name);
  
}




/* conditional macros
 *
 */



void if_macro_exists(data_buffer **out)
{
  int macro_name,
      macro_name_len,
      macro_exist;
  uint8_t *name;
  status_bitap *vecset;
  int name_len;
  ret_find_macro ret_macro;
  arg_text_return ret;


  /* first argument is the name of the macro */
  ret = argument_text(0, 1);

  name = ret.str_p;

  name_len = ret.length;

  macro_name = str_to_charstr(ret.str_p, ret.length);

  macro_name_len = (charstr[macro_name]).data.size;

  if(debug)
  {
    printf(" if macro exists macro name %.*s %i, %.*s len: %i\n", ret.length, ret.str_p, macro_name, name_len, name, macro_name_len);
  }

  
  /* fourth argument is the optional vector set */
  ret = argument_text(0, 4);

  if(ret.length > 0)
  {
    vecset = find_vectorset(ret.str_p, ret.length);
    if(vecset == NULL)
    {
      print_warning(Exit_user, "macro set: %.*s is incorrect. Using default.\n", ret.length, ret.str_p);
      vecset = current_status_bitap;
    }
    
  }
  else
  {
    vecset = current_status_bitap;
  }
  

  if((macro_name_len > 0) && (macro_name_len <= 64))
  {
    /* the length of the macro is correct */

    /* does the macro exist? */
    ret_macro = find_existing_macro(vecset, name, macro_name_len, name_len);
    macro_exist = ret_macro.macro_pos;

    if(debug)
    {
      printf("found macro: %i\n", macro_exist);
    }
    
    if(macro_exist >= 0)
    {
      output_arg(2, out); /* macro exists */
    }
    else
    {
      output_arg(3, out); /* macro does not exist */
    }
  }
  else
  {
    if(macro_name_len == 0)
    {
      output_arg(3, out); /* macro does not exist */
    }   
    else
    {
      print_warning(Exit_user, "length: %i macro name: %.*s  is incorrect. Can not check if macro is defined.\n", macro_name_len, name_len, name);
    }
  }

  /* remove macro name from memory */
  reduce_charstr(macro_name);
  
}


void set_macroset(data_buffer **out)
{
  arg_text_return ret;
 
  
  /* first argument is the vectorset name */
  ret = argument_text(0, 1);

  select_vectorset(ret.str_p, ret.length);
}    


/* settings macros
 *
 */


void define_specialchar(data_buffer **out)
{
  uint8_t special;
  arg_text_return ret;

  
  /* first argument is a single char */
  ret = argument_text(0, 1);

  /* this argument should have length 1 */
  if(ret.length >= 1)
  {
    special = ret.str_p[0]; /* only the first char is relevant */

    /* second argument contains the string for charstring */
    ret = argument_text(0, 2);

    defined_charstr[special] = str_to_charstr(ret.str_p, ret.length);
    
    if(debug)
    {
     printf(" defined special char: %c\n", special);
    }
  }
  
}    


void define_pattern_chars(data_buffer **out)
{
  arg_text_return ret;

  
  /* first argument contains the chars */
  ret = argument_text(0, 1);


  /* string should have length 8 otherwise it is not valid */
  if(ret.length == 9)
  {
    set_charstr_chars(current_charstr_chars, ret.str_p[0], ret.str_p[1], ret.str_p[2], ret.str_p[3], ret.str_p[4], ret.str_p[5], ret.str_p[6], ret.str_p[7], ret.str_p[8]);
  }
  else
  {
    print_warning(Exit_user, "invalid length of string: %.*s to define characters for pattern, this results in no change.\n", ret.length, ret.str_p);
  }
  
  
}    


void define_arg_chars(data_buffer **out)
{
  arg_text_return ret;
  status_bitap *vecset;
  int num;
 
  
  /* fifth argument is the optional vectorset name */
  ret = argument_text(0, 5);
  
  if(ret.str_p != NULL)
  {
    vecset = find_or_new_vectorset(ret.str_p, ret.length);
  }
  else
  {
    vecset = current_status_bitap;
  }

  /* first argument contains the chars */
  ret = argument_text(0, 1);

  num = asciitohex[ret.str_p[0]];
  if( ((num >= 1) && (num <= 5)) || ((num >= 10) && (num <= 13)) )
  {
    vecset->num_digits = num;
  }
  else
  {
    vecset->num_digits = 1;
    print_warning(Exit_user, "number of digits is out of range. Using 1.\n");
  }

 
  /* string should have length 5 or 6 otherwise it is not valid */
  if(ret.length == 5)
  {

    set_arg_chars(vecset->argchars, ret.str_p[1], ret.str_p[2], ret.str_p[3], ret.str_p[4], '\0');

    add_arg_to_vectors(vecset->vec, vecset->argchars);

  }
  else
  {
    if(ret.length == 6)
    {
      set_arg_chars(vecset->argchars, ret.str_p[1], ret.str_p[2], ret.str_p[3], ret.str_p[4], ret.str_p[5]);

      add_arg_to_vectors(vecset->vec, vecset->argchars);

    }


    else
    {
      print_warning(Exit_user, "invalid length of string: %.*s to define characters for arguments results in no change.\n", ret.length, ret.str_p); 
    }
  }
  
  /* second arg = variable number for start of quote */
  num = (int) argument_num(0, 2);
  if((num > 0) && (num < max_number_variables))
  {
    vecset->quote_var_start = num; 
  }
  else
  {
    print_warning(Exit_user, "variable number: %i is out of range.\n", num);
  }

  /* third arg = variable number for end of quote */
  num = (int) argument_num(0, 3);
  if((num > 0) && (num < max_number_variables))
  {
    vecset->quote_var_end = num; 
  }
  else
  {
    print_warning(Exit_user, "variable number: %i is out of range.\n", num);
  }

  /* fourth arg = variable number for separator of quote */
  num = (int) argument_num(0, 4);
  if((num > 0) && (num < max_number_variables))
  {
    vecset->quote_var_separator = num; 
  }
  else
  {
    print_warning(Exit_user, "variable number: %i is out of range.\n", num);
  }


}    


/* execute shell macro
 *
 */


void exec_command(data_buffer **out)
{
  arg_text_return ret;
  sds command;
  FILE *io;
  char *io_buffer;
  int io_size;
  size_t io_buffer_size;
  
  /* first argument contains the string to execute */
  ret = argument_text(0, 1);

  command = sdsnewlen(ret.str_p, ret.length);


  io_buffer = xmalloc(command_io_buffer);
  io_buffer_size = command_io_buffer;
  
  io = popen(command, "r");
  if (io == NULL)
  {
    print_warning(Exit_io, "could not execute: %s; %s\n", command, strerror(errno));
    exit(Exit_io);
  }
  
  do
  {
    io_size = getline(&io_buffer, &io_buffer_size, io);
    if(io_size > 0)
    {
      putchars_buffer(io_buffer, io_size, out);
    }
  } while(io_size > 0);
  
  sysreturn = pclose(io);
  
  xfree(io_buffer);
  sdsfree(command);
}


/* io macros
 *
 */

void tempfile(data_buffer **out)
{
  arg_text_return ret;
  int filetemp;
  sds template;
  int i,
      countx;
  char X = 'X';
  
  
  /* first argument contains the template */
  ret = argument_text(0, 1);
  
  template = sdsnewlen(ret.str_p, ret.length);
  
  /* adapt template string if not enough X */
  i = ret.length - 1;
  countx = 0;
  while((template[i] == 'X') && (i > (ret.length - 7)))
  {
    i--;
    countx++;
  }
  
  while(countx < 6)
  {
    template = sdscatlen(template, &X, 1);
    countx++;
  }
  
  filetemp = mkstemp(template);
  
  if (filetemp < 0)
  {
    print_warning(Exit_io, "can not create temporary file: %s: %s.\n", template, strerror(errno));
    exit(Exit_io);
  }
  else
  {
    if(close(filetemp) < 0)
    {
      print_warning(Exit_io, "Error closing file: %s: %s.\n", template, strerror(errno));
      exit(Exit_io);
    }

  }
  
  putchars_buffer(template, sdslen(template), out);
  
  sdsfree(template);
}


void include_file_ornot(data_buffer **out, int silent)
{
  arg_text_return ret;
  sds filename;
  
  /* first argument contains the filename */
  ret = argument_text(0, 1);
  
  
  /* check if argument exists */
  if(ret.length > 0)
  {
    filename = sdsnewlen(ret.str_p, ret.length);
    
    /* open the input file */
    if(open_input_silent(filename, *out, silent) == 0)
    {
      /* increase the buffer size to an io biffer size */
      *out = reserve_buffer(*out, input_buffer_size);
      
      /* read the first input to fill the buffer */
      read_input(out, 0);
      /* need to set position, because this is expected to be like this
       * in the exec_macro function
       */ 
      (*out)->position = (*out)->length;
      
      /* and set current input file */
      (*out)->prev = current_input_file_buffer;
      current_input_file_buffer = *out;
      
    }
    /* else no input file */
    
  }
  else
  {
    if(silent == 0)
    {
      print_warning(Exit_user, "no filename for include file.\n");
    }
  }
  
}    


void include_file_ornot2(data_buffer **out, int silent)
{
  arg_text_return ret;
  sds filename;
  int filedesc,
      filesize,
      read_bytes;
  
  /* first argument contains the filename */
  ret = argument_text(0, 1);


  /* check if argument exists */
  if(ret.length > 0)
  {
    filename = sdsnewlen(ret.str_p, ret.length);
    
    filedesc = open_silent(filename, silent);
    
    if(filedesc >= 0)
    {
      filesize = size_of_file(filedesc);
      
      if(filesize >= 0)
      {
        /* get enough room for data in out buffer */
        *out = reserve_buffer(*out, filesize);
        
        read_bytes = read(filedesc, &((*out)->data[(*out)->position]), filesize);
        
        (*out)->position += read_bytes;
        
        if (read_bytes < 0)
        {
          fprintf(stderr, "Error reading file: %s %i: %s.\n", filename, filedesc, strerror(errno));
          exit(Exit_io);
        }
        
        
        /* close file */
        if(close(filedesc) < 0)
        {
          fprintf(stderr, "Error closing file: %s: %s.\n", filename, strerror(errno));
          exit(Exit_io);
        }
      }
      /* else not a real file?, what to do? */
      
    }
    /* else could not open file */
    
    sdsfree(filename);
  }
  else
  {
    if(silent == 0)
    {
      print_warning(Exit_user, "no filename for include file.\n");
      // exit_code = Exit_user;
    }
  }

}    


void include_file(data_buffer **out)
{

  include_file_ornot(out, 0);

}

void include_file_silent(data_buffer **out)
{

  include_file_ornot(out, 1);

}


void divert(data_buffer **out)
{
  int ret;

  if(current_status_pattern->num_of_args == 0)
  {
    ret = 0;
  }
  else
  {
    /* first argument is the number of the diversion */
    ret = (int) argument_num(0, 1);
  }

  /* only set diversion
   * The real work is done in the output handling.
   */
   (*out)->divnum = ret;

   if(debug)
   {
     printf(" diversion is set: %i\n", ret);
   }
}


void undivert(data_buffer **out)
{
  long long int ret;
  int i,
      num_args;

  num_args = current_status_pattern->num_of_args;

  for(i = 0; i < num_args; i++)
  {
    /*  argument is the number of the diversion */
    ret = argument_num(0, i + 1);

    if(((*out)->divnum != ret) && (ret > 0))
    {
      flush_diversion(ret, out);
    }

    if(debug)
    {
     printf(" undivert: %lli\n", ret);
    }
  }

  if(num_args == 0)
  {
    flush_all_diversions(out);
  }

}


void at_last(data_buffer **out)
{
  arg_text_return ret;
  int i,
      num_args;

  num_args = current_status_pattern->num_of_args;

  for(i = 0; i < num_args; i++)
  {
    /*  argument is the text to be stored */
    ret = argument_text(0, i + 1);

    write_in_at_last(ret.str_p, ret.length);
    if(i < (num_args - 1))
    {
      write_in_at_last(" ", 1); /* and a space as m4 does */
    }
  }

}


void at_first(data_buffer **out)
{
  arg_text_return ret;
  int i,
      num_args;

  num_args = current_status_pattern->num_of_args;

  for(i = 0; i < num_args; i++)
  {
    /*  argument is the text to be stored */
    ret = argument_text(0, i + 1);

    write_in_at_first(ret.str_p, ret.length);
  }

}

void print_error(data_buffer **out)
{
  arg_text_return ret;
  int i,
      num_args;

  num_args = current_status_pattern->num_of_args;

  for(i = 0; i < num_args; i++)
  {
    /*  argument text */
    ret = argument_text(0, i + 1);

    fwrite(ret.str_p, 1, ret.length, stderr);
    fwrite(" ", 1, 1, stderr);

  }

}

void exit_really(data_buffer **out)
{
  int ret;

  /* first argument is the number of the errorcode */
  ret = (int) argument_num(0, 1);

  if((ret < 0) || (ret > 255))
  {
    ret = 1;
  }

  exit(ret);

}

/* variables macros
 *
 */

void set_var(data_buffer **out)
{
  long long int ret;
  arg_text_return retstr;


  /* first argument is the number of the variable */
  ret = argument_num(0, 1);

  /* the number of the variable should be larger than 0 and smaller than max */
  if((ret > 0) && (ret < max_number_variables))
  {
    /* second argument contains the string for the variable */
    retstr = argument_text(0, 2);

    if(variables[ret] != NULL)
    {
      variables[ret] = sdscpylen(variables[ret], retstr.str_p, retstr.length);
    }
    else
    {
      variables[ret] = sdsnewlen(retstr.str_p, retstr.length);
    }
    
    if(debug)
    {
      printf(" defined variable: %lli as: %s\n", ret, variables[ret]);
    }
  }
  else
  {
    print_warning(Exit_user, "variable number: %lli is out of range.\n", ret);
  }

}


void get_var(data_buffer **out)
{
  long long int ret;
  int len;
  sds value;

  /* first argument is the number of the variable */
  ret = argument_num(0, 1);

  /* the number of the variable should be larger than 0 and smaller than max */
  if((ret > 0) && (ret < max_number_variables))
  {
    if(variables[ret] != NULL)
    {
      len = sdslen(variables[ret]);

      putchars_buffer(variables[ret], len, out);
    }
  }
  else
  {
    /* using negative numbers for specific data from the program, process or environment */
    if((ret < 0) && (ret >= -9))
    {
      switch(ret)
      {
        case -1:
          /* diversion number */
          value = sdsfromlonglong((long long int) (*out)->divnum);
          set_argument(0, 2, out, (*out)->position, sdslen(value));
          putchars_buffer(value, sdslen(value), out);
          sdsfree(value);
          break;
        case -2:
          /* os */
          set_argument(0, 2, out, (*out)->position, 4);
          putchars_buffer("unix", 4, out);
          break;
        case -3:
          /* line number */
          value = sdsfromlonglong((long long int) line_counter);
          set_argument(0, 2, out, (*out)->position, sdslen(value));
          putchars_buffer(value, sdslen(value), out);
          sdsfree(value);
          break;
        case -4:
          /* local line number */
          value = sdsfromlonglong((long long int) local_line_counter);
          set_argument(0, 2, out, (*out)->position, sdslen(value));
          putchars_buffer(value, sdslen(value), out);
          sdsfree(value);
          break;
        case -5:
          /* current file name */
          set_argument(0, 2, out, (*out)->position, strlen(current_input_file_buffer->filename));
          putchars_buffer(current_input_file_buffer->filename, strlen(current_input_file_buffer->filename), out);
          break;
        case -6:
          /* program name */
          set_argument(0, 2, out, (*out)->position, strlen(program_name));
          putchars_buffer(program_name, strlen(program_name), out);
          break;
        case -7:
          /* program options after -- */
          set_argument(0, 2, out, (*out)->position, sdslen(arg_options));
          putchars_buffer(arg_options, sdslen(arg_options), out);
          break;
        case -8:
          /* program options after -- just before the input file */
          set_argument(0, 2, out, (*out)->position, sdslen(arg_options_local));
          putchars_buffer(arg_options_local, sdslen(arg_options_local), out);
          break;
        case -9:
          /* the return value of an executed shell command */
          value = sdsfromlonglong((long long int) sysreturn);
          set_argument(0, 2, out, (*out)->position, sdslen(value));
          putchars_buffer(value, sdslen(value), out);
          sdsfree(value);
          break;
      }
    }
    else
    {
      print_warning(Exit_user, "variable number: %lli is out of range.\n", ret);
    }
  }
}


/* string macros
 *
 */

void string_index(data_buffer **out)
{
  arg_text_return main_string,
                  sub_string;
  long long int index;
  void *pos;
  sds value;

  main_string = argument_text(0, 1);

  sub_string = argument_text(0, 2);

  pos = memmem(main_string.str_p, main_string.length, sub_string.str_p, sub_string.length);

  if(pos == NULL)
  {
    index = -1;
  }
  else
  {
    index = pos - (void *) main_string.str_p;
  }

  value = sdsfromlonglong(index);
  set_argument(0, 3, out, (*out)->position, sdslen(value));
  putchars_buffer(value, sdslen(value), out);
  sdsfree(value);

}


void num_to_char(data_buffer **out)
{
  long long int num;
  int num_numbers,
      i;
  uint8_t character;

  num_numbers = current_status_pattern->num_of_args;

  set_argument(0, 2, out, (*out)->position, num_numbers);
  
  for(i = 0; i < num_numbers; i++)
  {
    
    num = argument_num(0, i + 1);
    
    if((num >= 0ll) && (num <= 255ll))
    {
      character = (uint8_t) num;
      putchar_buffer(character, out);
    }
    else
    {
      print_warning(Exit_user, "number: %lli for character is out of range. Output is space.\n", num);
      putchar_buffer(' ', out);
      
      
    }
  }
  
}


void string_substr(data_buffer **out)
{
  arg_text_return main_string;
  long long int from,
                length;
  uint8_t *start;

  main_string = argument_text(0, 1);

  from = argument_num(0, 2);


  if((from < main_string.length) && (from >= 0))
  {
    if(current_status_pattern->num_of_args <= 2)
    {
      length = main_string.length - from;
    }
    else
    {
      length = argument_num(0, 3);

      if((length + from) > main_string.length)
      {
        length = main_string.length - from;
      }

    }
    start = main_string.str_p + from;

    set_argument(0, 4, out, (*out)->position, length);
    putchars_buffer(start, length, out);
  }

}


short int next_char_string(short int from, uint8_t *str, int len, int *pos_from)
{
  short int from_start,
            from_end,
            from_char;
  
  // fprintf(stderr, "trace  from %hi, pos from %i.\n", from, *pos_from);

  if(str != NULL)
  {
    from_char = (short int) str[*pos_from];
  }
  else
  {
    from_char = -1;
  }
  
  // fprintf(stderr, "trace b from %hi, from start %hi, from end, fromchar -%hi-.\n", from, from_start, from_end, from_char);

  if(*pos_from < len)
  {
    if((from_char == (short int) '-') && (*pos_from > 0) && (*pos_from < (len - 1)))
    {
      /* yes there is a range */
      from_start = str[*pos_from - 1];
      from_end = str[*pos_from + 1];
      
      if( from_end >= from_start)
      {
        /* increasing */
        if(from < from_end)
        {
          from++;
        }
      }
      else
      {
        /* decreasing */
        if(from > from_end)
        {
          from--;
        }
      }
      
      if(from == from_end)
      {
        *pos_from += 2;
      }
    }
    else
    {
      from = (int) from_char;
      (*pos_from)++;
    }
  }
  else
  {
    from = -1;
  }

  // fprintf(stderr, "trace e  from %hi, pos from %i from start %hi, from end %hi, fromchar -%hi-.\n", from, *pos_from, from_start, from_end, from_char);
  
  return(from);
}

void string_translate(data_buffer **out)
{
  arg_text_return main_string,
                  from_string,
                  to_string;
  short int trans[256];
  uint8_t transchar;
  uint8_t *string;
  int pos_from,
      pos_to,
      i,
      start_pos,
      arg_len;
  short int from = 0,
            to = 0;
        

  main_string = argument_text(0, 1);

  from_string = argument_text(0, 2);

  to_string = argument_text(0, 3);


  /* first fill the translation table */
  for(i = 0; i < 256; i++)
  {
    trans[i] = -2;
  }

  pos_from = 0;
  pos_to = 0;
  while(pos_from < from_string.length)
  {
    from = next_char_string(from, from_string.str_p, from_string.length, &pos_from);

    to = next_char_string(to, to_string.str_p, to_string.length, &pos_to);

    if(trans[from] == -2)
    {
      trans[from] = to;
    }
  }
  
  string = main_string.str_p;
  
  start_pos = (*out)->position;
  arg_len = 0;

  *out = reserve_buffer(*out, main_string.length);
  
  for(i = 0; i < main_string.length; i++)
  {

    if(trans[*string] >= 0)
    {
      transchar = (uint8_t) trans[*string];
      putchar_buffer(transchar, out);
      arg_len++;
    }
    else
    {
      if(trans[*string] == -2)
      {
        putchar_buffer(*string, out);
        arg_len++;
      }
    }
    
    string++;
  }

  set_argument(0, 4, out, start_pos, arg_len);

}


void number_to_string(data_buffer **out)
{
  long long int number,
                radix,
                width;
  long long unsigned int unumber;
  int i,
      num_args,
      mod_num,
      str_pos,
      neg,
      start;
  uint8_t str[64];   /* 64 is enough for binary the longest possible string */ 

  
  num_args = current_status_pattern->num_of_args;
              
  number = argument_num(0, 1);
 
  if(num_args >= 2)
  {
    radix = argument_num(0, 2);
  }
  else
  {
    radix = 10;
  }
  
  if(num_args >= 3)
  {
    width = argument_num(0, 3);
  }
  else
  {
    width = 0;
  }
  
  if(debug)
  {
    printf(" Number to string parameters value: %lli radix: %lli width: %lli\n", number, radix, width);
  }
 
  /* only if radix and width are within range then output */
  if((radix >= 2) && (radix <= 36) && (width >= 0))
  {
    
    if(number < 0)
    {
      neg = -1;
      unumber = - number;
    }
    else
    {
      neg = 1;
      unumber  = number;
    }
    
    
    /* pre part */
    start = (*out)->position;
    switch(radix)
    {
      case 2:
        putchars_buffer("0b", 2, out);
        break;
      case 16:
        putchars_buffer("0x", 2, out);
        break;
      case 8:
        putchar_buffer('0', out);
        break;
      case 10:
        break;
      default:
        putchars_buffer("0r", 2, out);
        if(radix > 9)
        {
          putchar_buffer(numtoascii[(radix / 10)], out);
        }
        putchar_buffer(numtoascii[(radix % 10)], out);
        putchar_buffer(':', out);
    }
    
    set_argument(0, 4, out, start, (*out)->position - start);
    
    str_pos = 63;
    do
    {
      mod_num = number % radix;
      str[str_pos] = numtoascii[mod_num];
      str_pos--;
      number /= (long long unsigned int) radix;
      // printf(" modulo: %i  num: %llu  c: %c\n", mod_num, number, str[str_pos + 1]);
    } while((number != 0) && (str_pos >= 0));
    
    /* output string of number in binary form */
    start = (*out)->position;
    putchars_buffer(&str[str_pos + 1], (63 - str_pos), out);
    set_argument(0, 5, out, start, (*out)->position - start);
    
    
    (*out)->start = (*out)->position; /* output of macro starts here */

    /* output negative */
    start = (*out)->position;
    if(neg == -1)
    {
      putchar_buffer('-', out);
    }
    set_argument(0, 6, out, start, (*out)->position - start);

    str_pos = 63;
    do
    {
      mod_num = unumber % radix;
      str[str_pos] = numtoascii[mod_num];
      str_pos--;
      unumber /= (long long unsigned int) radix;
      // printf(" modulo: %i  num: %llu  c: %c\n", mod_num, unumber, str[str_pos + 1]);
    } while((unumber != 0) && (str_pos >= 0));
    
    /* output leading zeros */
    start = (*out)->position;
    for(i = 0; i < (width - 63 + str_pos); i++)
    {
      putchar_buffer('0', out);
    }
    set_argument(0, 7, out, start, (*out)->position - start);
    
    /* output string of number */
    start = (*out)->position;
    putchars_buffer(&str[str_pos + 1], (63 - str_pos), out);
    set_argument(0, 8, out, start, (*out)->position - start);
    
  }
  else
  {
    if((radix < 2) || (radix > 36))
    {      
      print_warning(Exit_user, "radix: %lli is out of range.\n", radix);
      // exit_code = Exit_user;
    }
    if(width < 0)
    {      
      print_warning(Exit_user, "width: %lli is negative.\n", width);
      // exit_code = Exit_user;
    }
  }
  
}