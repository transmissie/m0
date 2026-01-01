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




/* define recursive types */
typedef enum
{
  Recursive_yes,
  Recursive_no
    
} macro_option_recursive;


/* the macro definition and all other info of the macro */
typedef struct macro_def
{
  macro_option_recursive recursive;  /* the result of the macro is used for macro expansion */
  run_macro arg_type;  /* are macros expanded during argument collection  */
  int builtin;      /* index to builtin macro functions */
  sds name;         /* the macro name from the definition of the macro, used to find similar in list */
  int name_len;      /* the length of the name as calculated for the vectors */
  int pre_size,     /* number of characters of the beginning of the macro name to return to output */ 
      post_size;    /* number of characters of the end of the macro name to return to input */
  sds def;          /* the definition string */
  int def_len;      /* the length of the definition */
  pattern_data *arglist;  /* pointer to the pattern vectors to be used for the argument */
  pattern_data *filllist;  /* pointer to the pattern vectors to be used for fill of definition */
  status_bitap *mcallset;  /* pointer to a vector set used by macro call */
  int program;      /* index to program to be executed after the arguments are collected */
  struct macro_def *prev; /* pointer to the previous definition */
  uint8_t virtual_char; /* optional virtual char to be used */
} macro_def;


/* following struct and union are used for holding the data string for macro names
 * and patterns.
 * It defines in an entry the start and end of a range of bytes for a position in
 * the macro name or pattern. Multiple ranges of bytes can be defined in multiple
 * entries for a single position.
 * 
 * Multiple entries defining a name are called here a charstr (character string).
 * A single charstr is defined as:
 * 
 * entry type           size                                  start       end
 *   1   start          length of string as input to vectors  na          na
 *   2   char/charincr  increment size: 0 / 1                 start byte  end byte
 *   3   additional chars if applicable
 *   4   end            na                                    na          na
 * 
 *  The increment determines if the next entry should start at the next position.
 */


typedef struct
{
  uint8_t type;     /* type of this char */
  uint8_t size;     /* size or increment */
  uint8_t start;    /* start of range */
  uint8_t end;      /* end of range */
} charrange;  

typedef union
{
  uint32_t complete;
  charrange data;
} char_range;


/* next defines used for type in charrange */
/* start of charstring */
#define charrtype_start  's'   
/* entry with character with next entry a new character */ 
#define charrtype_charincr  'n'
/* entry to append a character to current position */
#define charrtype_char  'a'

/* specials for argument collection */
/* entry with character for + with always next entry a new character */ 
#define charrtype_oneormoreincr  '+'
/* entry with character for * with always next entry a new character */ 
#define charrtype_zeroormoreincr  '*'
/* entry with character for ? with always next entry a new character */ 
#define charrtype_zerooroneincr  '?'
/* entry with character for trigger with always next entry a new character */ 
#define charrtype_trigincr  't'

/* end of charstring */
#define charrtype_end  'e'



typedef struct 
{
  uint8_t  start_col,  /* char to indicate the start of a collection of chars  */ 
         end_col,      /* char to indicate the end of a collection of chars */ 
         range_char,   /* char to indicate a range of chars */ 
         charstr_char, /* char to insert defined charstr */
         number,       /* char to insert a char with hex number */
         oneormore,    /* char to indicate at the end of a collection one or more times */
         zeroormore,   /* char to indicate at the end of a collection zero or more times */
         zeroorone,    /* char to indicate at the end of a collection zero or one time */
         trig;        /* char to indicate the end of a collection a trigger */
} charstr_chars;



/* types for list of builtin macro functions */
typedef enum
{
  nothing_to_do,
  update_input_buffer
} macro_return_code;

typedef union
{
  uint8_t  n8[8];
  uint64_t n64;
} builtin_name;

typedef struct
{
  builtin_name name;
  void (*intern)(data_buffer **);  /* a pointer to the builtin macro function */
} builtins;


extern sds variables[max_number_variables];

extern const builtins internal[];



extern char_range (*charstr);

extern charstr_chars (*current_charstr_chars);

extern macro_def (*macro_list);

charstr_chars *init_charstr_chars(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);

void init_macros(void);

void init_asciitohex(void);

void init_charstr(void);

void count_macros(int macrocounters[max_size_macro + 1]);

int reduce_charstr(int);

int str_to_charstr(uint8_t *, int);

void add_pattern(data_buffer **);

void append_pattern(data_buffer **);

void clear_pattern(data_buffer **);

void copy_pattern(data_buffer **);

void push_macro(data_buffer **);

void define_macro(data_buffer **);

void def_macrocall(data_buffer **);

void push_macrocall(data_buffer **);

void pop_macro(data_buffer **);

void undefine_macro(data_buffer **);

void info_macro(data_buffer **);

void if_macro_exists(data_buffer **);

void set_macroset(data_buffer **);

void define_specialchar(data_buffer **);

void define_pattern_chars(data_buffer **);

void define_arg_chars(data_buffer **);

void set_var(data_buffer **);

void get_var(data_buffer **);

void exec_command(data_buffer **);

void tempfile(data_buffer **);

void include_file(data_buffer **);

void include_file_silent(data_buffer **);

void divert(data_buffer **);

void undivert(data_buffer **);

void at_last(data_buffer **);

void print_error(data_buffer **);

void exit_really(data_buffer **);

void string_index(data_buffer **);

void num_to_char(data_buffer **);

void string_substr(data_buffer **);

void string_translate(data_buffer **);

void number_to_string(data_buffer **);

