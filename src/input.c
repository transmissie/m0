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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "definesizes.h"
#include "exitcodes.h"
#include "input.h"
#include "xmalloc.h"



/* counters for the lines */
int line_counter = 0,
    local_line_counter = 0;
    
// char *current_open_file;

char stdin_file_name[] = "stdin";

char *program_name;

char *arg_options;

data_buffer *current_input_file_buffer;



data_buffer *alloc_io_buffer(size_t datasize)
{
  data_buffer *buffer;
 
  buffer = xmalloc(sizeof(data_buffer) + datasize);
  
  buffer->size = datasize;
  buffer->prev = NULL;  /* set default to not having a previous buffer */
  buffer->position = 0;  
  buffer->length = 0;  
  buffer->divnum = 0;  
  buffer->start = 0;  

  if(debug)
  {
    printf("step aloc %i\n", buffer->size);
  }
  
  return(buffer);
}


data_buffer *incr_buffer(data_buffer *buf, size_t increase)
{
  data_buffer *buffer;
  size_t new_size;
  
  new_size = buf->size + increase;

  buffer = xrealloc(buf, sizeof(data_buffer) + new_size);
  
  buffer->size = new_size;
      
  return(buffer);
}


data_buffer *reserve_buffer(data_buffer *buf, int extra)
{
  /* get enough room for data in out buffer */
  if((buf->position + extra) > buf->size)
  {
    /* increase room in out buffer */
    buf = incr_buffer(buf, extra + buf->position - buf->size);
  }   
  
  return(buf);
}


int open_silent(char *filename, int silent)
{
  int file;

  if(filename[0] == '-' && filename[1] == '\0')
  {
    /* stdin as input */
    file = STDIN_FILENO;
    // current_open_file = stdin_file_name;
  }
  else
  {
    /* open file for input */
    file = open(filename, O_RDONLY);
    if (file < 0)
    {
      if(silent == 0)
      {
        fprintf(stderr, "Error; can not open file: %s: %s.\n", filename, strerror(errno));
        exit(Exit_io);
      }

    }
    else
    {
      // current_open_file = filename;
    }
  }

  return(file);
}

off_t size_of_file(int filedesc)
{
  off_t current,
        size;
  
  /* to be safe get the current position */
  current = lseek(filedesc, 0, SEEK_CUR);
  if(current < 0)
  {
    /* can not do seek, maybe stdin? */
    size = -1;
  }
  else
  {
  /* get the size by the end position */
  size = lseek(filedesc, 0, SEEK_END);
  /* set the position back */
  lseek(filedesc, current, SEEK_SET);
  }
  
  return(size);
}


int open_input_silent(char *filename, data_buffer *buffer, int silent)
{
  int ret;

  ret = 0;

  if(filename[0] == '-' && filename[1] == '\0')
  {
    /* stdin as input */
    buffer->file = STDIN_FILENO;
    buffer->filename = stdin_file_name;
  }
  else
  {
    /* open file for input */
    buffer->file = open(filename, O_RDONLY);
    if (buffer->file < 0)
    {
      ret = 1;
      if(silent == 0)
      {
        fprintf(stderr, "Error; can not open file: %s: %s.\n", filename, strerror(errno));
        exit(Exit_io);
      }

    }
    else
    {
      // current_open_file = filename;
    }
  }

  if(ret == 0)
  {
    buffer->filename = filename;

    local_line_counter = 1;
  }

  return(ret);
}

  
void open_input(char *filename, data_buffer *buffer)
{
  
  if(filename[0] == '-' && filename[1] == '\0')
  {
    /* stdin as input */
    buffer->file = STDIN_FILENO;
    buffer->filename = stdin_file_name;
  }
  else
  {
    /* open file for input */
    buffer->file = open(filename, O_RDONLY);
    if (buffer->file < 0)
    {
      fprintf(stderr, "Error; can not open file: %s: %s.\n", filename, strerror(errno));
      exit(Exit_io);
    }
    // current_open_file = filename;
  }

  buffer->filename = filename;

  local_line_counter = 1;
  
}

void close_input(data_buffer *buffer)
{
  
  if((buffer->file != STDIN_FILENO) && (buffer->file > 0)) /* if stdin or not a file do not close */
  {
    /* close file */
    if(close(buffer->file) < 0)
    {
      fprintf(stderr, "Error closing file: %s: %s.\n", buffer->filename, strerror(errno));
      exit(Exit_io);
    }
  }

  // buffer->file = -1; /* just to be sure */
  
}

int read_input(data_buffer *buffer, int reserve)
{
  int req_bytes,
      read_bytes,
      i, j;
  
  if(buffer->position > reserve)
  {
    /* copy reserved bytes to the beginning of the buffer */
    j = 0;
    for(i = (buffer->position - reserve); i < buffer->position; i++)
    {
      buffer->data[j] = buffer->data[i];
      j++;
    }
    buffer->position = reserve;
  }

  req_bytes = buffer->size - buffer->position;
    
  read_bytes = read(buffer->file, &buffer->data[buffer->position], req_bytes);
  
  buffer->length = buffer->position + read_bytes;
  
  if (read_bytes < 0)
  {
    fprintf(stderr, "Error reading file: %s %i: %s.\n", buffer->filename, buffer->file, strerror(errno));
    exit(Exit_io);
  }


  return(read_bytes);

}

void clear_buffer(data_buffer *buffer, int reserve)
{
  int i, j;

  if(buffer->position > reserve)
  {
    /* copy reserved bytes to the beginning of the buffer */
    j = 0;
    for(i = (buffer->position - reserve); i < buffer->position; i++)
    {
      buffer->data[j] = buffer->data[i];
      j++;
    }
    buffer->position = reserve;
  }

}


void print_bits(long long int value)
{
  int i;

  printf(" bitpattern: 0b");

  for(i=0; i < 64; i++)
  {
    if((value & (0b1ll << 63)) == 0ll)
    {
      putchar('0');
    }
    else
    {
      putchar('1');
    }
    value <<= 1;
  }

  putchar('\n');

}
