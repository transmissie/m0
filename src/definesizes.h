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

/* debugging on !=0 or off == 0 */
#define debug 0
#define debug_stack 0
#define debug_stepstack 0
#define stepdebug 0
#define stephistory 0
#define debug_statistics 0



/* sizes of io and memory buffers
 * increase to potentially increase performance
 * at the cost of more memory use
 */

#define input_buffer_size 512
#define input_buffer_size_reserve 200

#define init_size_processbuf 128
#define add_size_processbuf 1024 

#define output_buffer_size 512
#define output_buffer_size_reserve 200

#define div_entry_size 1024

#define command_io_buffer 128

#define size_history_checks 4096
#define size_history_chars 1024
#define size_reduce_history_chars 500


/* sizes of internal tables
 * normally you should not change these
 */

#define size_checks_mem 200

#define init_size_macro_list 256
#define add_size_macro_list 256

#define init_size_macropart_list 256
#define add_size_macropart_list 256

#define init_size_charstr 256
#define add_size_charstr 256

#define init_size_stack 256
#define add_size_stack 256

#define init_size_program_list 1024
#define add_size_program_list 1024

#define init_size_name_of_programs 32
#define add_size_name_of_programs 64

#define init_size_tag_list 64
#define add_size_tag_list 64

#define init_size_div_list 16
#define add_size_div_list 64

#define size_init_ordered_opt 10
#define size_add_ordered_opt 10

/* sizes determined by selected functionality
 * these should not be changed 
 */

#define max_recursion_depth 20000

#define size_defcharstr 256

#define number_of_default_stacks 8

#define max_number_variables 256

#define max_number_counters 256

/* used for writing from processbuf and reading from input buffer **/
#define max_size_macro 64
