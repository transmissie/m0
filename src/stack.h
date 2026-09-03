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



typedef struct
{
    uint8_t *str_p;    /* pointer to beginning of text */
    int length;        /* lenght of text */
} arg_text_return;

typedef enum
{
    arg_continu,
    arg_stop,
    arg_abort,
    arg_no_macros
} arg_status;

typedef struct
{
    arg_status status;
    // int replace_backup,  /* positions to go back to replace text in output */
        // replace_len;
    // sds replace_text;
    int pattern_len;     /* length of the triggered pattern part */
    int goback;    /* length to go back in the input and output after a stop */ 
} arg_run;

/* only extern because used for statistics */
extern int size_program_list,
           end_program_list;

/* used by processor.c */
extern int macro_depth;

void init_program_list(void);

void init_stacks(void);

void init_asciitoradix(void);

void add_to_programs(uint8_t *, int, int);

void start_local_stacks(void);

void change_arg_stack_start(int);

void end_local_stacks(void);

int get_size_stack(int);

int str_to_commands(uint8_t *, int);

int push_text(int, data_buffer **, int, int);

int push_str(int, uint8_t *, int);

int push_num(int, long long int);

long long int argument_num(int, int);

arg_text_return argument_text(int, int);

void set_argument(int, int, data_buffer **, int, int);

int st_nop(int, int, status_pattern *, data_buffer **, arg_run *);

int st_pop(int, int, status_pattern *, data_buffer **, arg_run *);

int st_pop_to(int, int, status_pattern *, data_buffer **, arg_run *);

int st_dup(int, int, status_pattern *, data_buffer **, arg_run *);

int st_swap(int, int, status_pattern *, data_buffer **, arg_run *);

int st_swap12(int, int, status_pattern *, data_buffer **, arg_run *);

int st_copy(int, int, status_pattern *, data_buffer **, arg_run *);

int st_copyfrom(int, int, status_pattern *, data_buffer **, arg_run *);

int st_get_arg(int, int, status_pattern *, data_buffer **, arg_run *);

int st_get_arg_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_get_from_out_opt(int, int, status_pattern *, data_buffer **, arg_run *);

int st_get_from_out_opt_last(int, int, status_pattern *, data_buffer **, arg_run *);

int st_replace_out_opt(int, int, status_pattern *, data_buffer **, arg_run *);

int st_replace_out(int, int, status_pattern *, data_buffer **, arg_run *);

int st_replace_out_start(int, int, status_pattern *, data_buffer **, arg_run *);

int st_end_begin(int, int, status_pattern *, data_buffer **, arg_run *);

int st_push_toarg(int, int, status_pattern *, data_buffer **, arg_run *);

int st_push_toarg_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_pushvar(int, int, status_pattern *, data_buffer **, arg_run *);

int st_beginarg(int, int, status_pattern *, data_buffer **, arg_run *);

int st_endarg(int, int, status_pattern *, data_buffer **, arg_run *);

int st_argposition(int, int, status_pattern *, data_buffer **, arg_run *);

int st_argnumber(int, int, status_pattern *, data_buffer **, arg_run *);

int st_macro_depth(int, int, status_pattern *, data_buffer **, arg_run *);

int st_overrule(int, int, status_pattern *, data_buffer **, arg_run *);

int st_if_cmp_set(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cmp_string(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cmp_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_greater_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_greaterequal_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_smaller_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_smallerequal_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_compare_number(int, int, status_pattern *, data_buffer **, arg_run *);

int st_if(int, int, status_pattern *, data_buffer **, arg_run *);

int st_else(int, int, status_pattern *, data_buffer **, arg_run *);

int st_while(int, int, status_pattern *, data_buffer **, arg_run *);

int st_endwhile(int, int, status_pattern *, data_buffer **, arg_run *);

int st_add(int, int, status_pattern *, data_buffer **, arg_run *);

int st_modulo(int, int, status_pattern *, data_buffer **, arg_run *);

int st_divide(int, int, status_pattern *, data_buffer **, arg_run *);

int st_multiply(int, int, status_pattern *, data_buffer **, arg_run *);

int st_power(int, int, status_pattern *, data_buffer **, arg_run *);

int st_bit_not(int, int, status_pattern *, data_buffer **, arg_run *);

int st_log_not(int, int, status_pattern *, data_buffer **, arg_run *);

int st_shift_left(int, int, status_pattern *, data_buffer **, arg_run *);

int st_shift_right(int, int, status_pattern *, data_buffer **, arg_run *);

int st_bit_logic(int, int, status_pattern *, data_buffer **, arg_run *);

int st_logic(int, int, status_pattern *, data_buffer **, arg_run *);

int st_ifthen(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cnt_get(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cnt_set(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cnt_clr(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cnt_incr(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cnt_decr(int, int, status_pattern *, data_buffer **, arg_run *);

int st_cat(int, int, status_pattern *, data_buffer **, arg_run *);

int st_str_multiply(int, int, status_pattern *, data_buffer **, arg_run *);

int st_strlen(int, int, status_pattern *, data_buffer **, arg_run *);

int st_setstack(int, int, status_pattern *, data_buffer **, arg_run *);

int st_setbase(int, int, status_pattern *, data_buffer **, arg_run *);

int st_base_option(int, int, status_pattern *, data_buffer **, arg_run *);

int st_op_stack_ex_if(int, int, status_pattern *, data_buffer **, arg_run *);

int st_op_stack_ex(int, int, status_pattern *, data_buffer **, arg_run *);

int st_op_stack_ex_to(int, int, status_pattern *, data_buffer **, arg_run *);

int st_op_stack_push(int, int, status_pattern *, data_buffer **, arg_run *);

int st_funtion_call(int, int, status_pattern *, data_buffer **, arg_run *);

int st_go_back(int, int, status_pattern *, data_buffer **, arg_run *);

int st_no_macro_exec(int, int, status_pattern *, data_buffer **, arg_run *);

int st_set_stack_free(int, int, status_pattern *, data_buffer **, arg_run *);

int st_subroutine(int, int, status_pattern *, data_buffer **, arg_run *);

int st_macro_num(int, int, status_pattern *, data_buffer **, arg_run *);

int st_macroinfo(int, int, status_pattern *, data_buffer **, arg_run *);

arg_run exec_program(int, int, status_pattern *, data_buffer **);

void print_program(int, data_buffer **);

sds sds_print_program(sds, int);
