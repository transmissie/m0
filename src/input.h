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

#include <stdint.h>
#include "sds.h"


typedef struct data_buffer 
{
  struct data_buffer *prev;        /* pointer to a potential previous data buffer */
  int file;                        /* the file descriptor */
  char *filename;                  /* the file name opened for this buffer */
  int size;                        /* the size of the data buffer */ 
  int length;                      /* the amount of data at a certain moment */ 
  int position;                    /* current position in the data */
  int start;                       /* a position to start is optionally used */
  int divnum;                      /* diversion number, used in output */
  uint8_t data[];                  /* a block of data */
} data_buffer;

extern int line_counter,
           local_line_counter;
           
// extern char *current_open_file;

extern char *program_name;

extern sds arg_options;
extern sds arg_options_local;

extern data_buffer *current_input_file_buffer;


data_buffer *alloc_io_buffer(size_t);

data_buffer *incr_buffer(data_buffer *, size_t);

data_buffer *reserve_buffer(data_buffer *, int);

int open_silent(char *, int);

off_t size_of_file(int);

int open_input_silent(char *, data_buffer *, int);

void open_input(char *, data_buffer *);

void close_input(data_buffer *);

int read_input(data_buffer **, int);

void clear_buffer(data_buffer *, int);

void print_bits(long long int);

void print_buf_info(data_buffer *, FILE *);
