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


/* size of the array of bit vectors for the bitap; this should match the number represented by an input (= byte) */
#define size_index 256

/* space reserved for masks in pattern matching. 32 seems enough, can be 64 max. */
#define pattern_size_masks 64

typedef  uint64_t vectors[3][size_index];    /* the bit vectors for the bitap algorithm */

extern vectors  (*current_vec);

// typedef  uint64_t checks[3];    /* the register used to hold the bit pattern in the bitap algorithm */

// extern checks (*current_check);

// extern uint64_t (*current_check)[];

extern const uint64_t init_vector[4];

extern const uint64_t mask_vector[4];

extern const uint64_t arg_vector[2];

extern const int l_to_vec[16];

extern const uint64_t mask_first [16];
  
extern const uint64_t mask_length [16];


/* The structure for storing the macro names and corresponding vectors for bitap algo.
 * This used as an array with length of word as index.
 * 
 * 
 * 
*/


typedef struct wordlist
{
  struct wordlist *next;  /* next wordlist */
  int word_size; /* only used to add correct size new wordlist */
  int num_words; /* last word used */
  int used_words; /* total used words <= num_words : to indicate empty words */
  int word_length[64];
  int macro[64]; /* index to list of macros */
  uint64_t vecs[][size_index];  /* e.g. when size = 15: 15*256*8 bytes = 30kb */
    
} wordlist;


extern wordlist (*current_word15);

extern wordlist (*current_word64);


typedef uint64_t pattern_vectors[][size_index]; /* bitap vectors */

typedef struct pattern_masks
{
  uint64_t init,                /* initial mask */
           mask,                /* mask to check result */
           starmask,            /* mask for one or more in one position */
           zeromask;            /* mask for zero (used together with one or more) in one position */
           // onetimemask_init;         /* mask for one time trigger */
  uint64_t masks[pattern_size_masks];            /* masks to check individual results */ 
  // uint64_t onetimemasks[pattern_size_masks];     /* masks to check individual results */
  int masks_end;                /* first free mask */
  // int onetimemasks_end;          /* first free mask */
  int masks_run[pattern_size_masks];             /* index to run pattern program */
  int masks_run_patlen[pattern_size_masks];     /* length of the pattern string; negative length means a one time trigger */
  int masks_run_level[pattern_size_masks];       /* level of pattern program */
  // int onetimemasks_run[pattern_size_masks];      /* index to run pattern program */
  // int onetimemasks_run_patlen[pattern_size_masks];     /* length of the pattern string */
  // int onetimemasks_run_level[pattern_size_masks]; /* level of pattern program */
} pattern_masks;

typedef struct pattern_data
{
  pattern_vectors (*vec);        /* bitap vectors */
  pattern_masks *masks;
  int vec_size;                 /* number of columns of bitap vectors */
  int end;                      /* first free bit position */
  sds name;                     /* name for reference to this pattern vector */
  struct pattern_data *prev;  /* pointer to previously defined pattern */
} pattern_data;

extern pattern_data *arglist,
                    *arglast;


typedef enum
{
  arg_sampling_started,
  no_arg_sampling
} status_arg_collecting;

typedef enum
{
  macro_setting_overrule_no,
  macro_setting_overrule_recursive,
  macro_setting_overrule_not_recursive  
} macro_settings_overrule;


typedef struct
{
  int start[number_of_default_stacks];         /* to set the start position for argument collection */
  int num_of_args;                             /* count of number of arguments collected by end instruction */
  int base_of_args;                            /* the basis for filling of args, can be set by command to emulate shift */
  macro_settings_overrule overrule;            /* setting for possible overruling of the recursive output of a macro */
  status_arg_collecting (stat_arg[number_of_default_stacks]); /* status of the collection of arguments, used for begin */
} status_pattern;
                      
extern status_pattern *current_status_pattern;

typedef struct
{
  uint8_t  first,  /* first character of the argument, after this: */
           all,    /* for all the arguments */
           allq,   /* same as above with arguments quoted */
           num,    /* amount of arguments */
           firstalt; /* first character of argument of stacks, further same as first */
} argument_chars;

extern argument_chars *current_arg_chars;

typedef struct status_bitap
{
  vectors  *vec;
  wordlist *word15;
  wordlist *word64;
  argument_chars *argchars;
  sds name;
  int quote_var_start,
      quote_var_end,
      quote_var_separator;
  struct status_bitap *prev; /* pointer to previous status */
  struct status_bitap *next; /* pointer to next status */
} status_bitap;

extern status_bitap *current_status_bitap;

/* extern for statistics */
extern status_bitap *first_status_bitap;

typedef enum
{
  pattern_no_append,
  pattern_append
} pattern_append_option;


extern int check_mem_end;


// uint64_t (*new_checks(int))[];

// void free_checks(int);

argument_chars *init_arg_chars(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);

wordlist *init_wordlist(int);

vectors *init_vectors(void);

status_bitap *new_vectorset(uint8_t *, int);

status_bitap *find_vectorset(uint8_t *, int);

status_bitap *find_or_new_vectorset(uint8_t *, int);

void select_vectorset(uint8_t *, int);

void set_vectorset(status_bitap *);

pattern_data *init_patternvectors(uint8_t *, int);

pattern_data *find_patternvec(uint8_t *, int);

void clear_patternvector(pattern_data (*vec), int);

void copy_patternvector(pattern_data (*vec_from), pattern_data (*vec_to));

void add_to_patternvector(pattern_data (*vec), int, int, int, pattern_append_option);

void add_arg_to_vectors(vectors (*vec), argument_chars *);

void add_to_vectors2(vectors (*vec), int);

int add_to_wordlist2(wordlist *, int, int);

void delete_from_wordlist(wordlist *, int);

// void rewind_check(int);




// void print_cur_check(void);

