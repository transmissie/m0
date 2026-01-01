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
#include <limits.h>
#include "definesizes.h"
#include "exitcodes.h"
#include "input.h"
#include "output.h"
#include "xmalloc.h"



typedef struct
{
  int divnum;    /* number of diversion */
  int length;    /* number of bytes in data */
  uint8_t data[div_entry_size]; /* data buffer */
} diversion_entry;


diversion_entry (*div_data)[];

int size_div_list,
    end_div_list,
    pos_div_list,
    at_last_div_list = -1;


int trace_file;
data_buffer *output_buffer;



void init_div_list(void)
{

  div_data = xmalloc(sizeof(diversion_entry) * init_size_div_list);

  size_div_list = init_size_div_list;
  end_div_list = 0;

}

void increase_div_list(void)
{
   if(end_div_list >= size_div_list)
    {
      /* increase div_list size */
      div_data = xrealloc(div_data, sizeof(diversion_entry) * (size_div_list + add_size_div_list));

      size_div_list += add_size_div_list;
    }
}

int new_diversion_entry(int divnum, int from)
{
  int i,
      found;

  if(from < 0)
  {
    from = 0;
  }

  /* search divdata with empty entry */
  found = -1;
  i = from;

  while((i < end_div_list) && (found < 0))
  {
    if((*div_data)[i].divnum == -1)
    {
        found = i;
    }
    i++;
  }

  if(found < 0)
  {
    /* no free entry, thus add new entry */
    found = end_div_list;

    end_div_list++;
    if(end_div_list > size_div_list)
    {
      increase_div_list();
    }
  }

  (*div_data)[found].divnum = divnum;
  (*div_data)[found].length = 0;

  return(found);
}


void open_diversion(int divnum, int from)
{
  int i,
      found,
      last_found_div;


  if(from < 0)
  {
    from = 0;
  }

  /* search divdata with room for new data */
  found = -1;
  last_found_div = -1;
  i = from;

  while((i < end_div_list) && (found < 0))
  {
    if((*div_data)[i].divnum == divnum)
    {
      last_found_div = i;
      if((*div_data)[i].length < div_entry_size)
      {
        found = i;
      }
    }
    i++;
  }

  if(found >= 0)
  {
    /* an entry has been found */
    if(divnum > 0)
    {
      pos_div_list = i;
    }
    else
    {
      at_last_div_list = i;
    }
  }
  else
  {
    if(divnum > 0)
    {
      pos_div_list = new_diversion_entry(divnum, last_found_div);
    }
    else
    {
      at_last_div_list = new_diversion_entry(divnum, last_found_div);
    }
  }

  if(debug)
  {
    printf("Open diversion num %i in list: %i\n", divnum, pos_div_list);
  }
  
}


data_buffer *putchars_buffer(uint8_t *in, int len, data_buffer *out)
{
  int j;

  for(j = 0; j < len; j++)
  {
    out->data[out->position] = *in;
    out->position++;
    in++;

    if(out->position >= out->size)
    {
      if(out->file >= 0)
      {
        /* output to real file or stdout */
        write_output(out, max_size_macro);
      }
      else
      {
        /* output is a memory buffer that needs to grow */
        out = incr_buffer(out, add_size_processbuf);
      }
    }
  }

  return(out);
}


data_buffer *putchar_buffer(uint8_t in, data_buffer *out)
{
  out->data[out->position] = in;
  out->position++;
  
  if(out->position >= out->size)
  {
    if(out->file >= 0)
    {
      /* output to real file or stdout */
      write_output(out, max_size_macro);
    }
    else
    {
      /* output is a memory buffer that needs to grow */
      out = incr_buffer(out, add_size_processbuf);
    }
  }
  
  return(out);
}



data_buffer *flush_diversion(int divnum, data_buffer *out)
{
  int i;

  for(i = 0; i < end_div_list; i++)
  {
    if((*div_data)[i].divnum == divnum)
    {
      out = putchars_buffer((*div_data)[i].data, (*div_data)[i].length, out);
      (*div_data)[i].divnum = -1;
      if(debug)
      {
        printf("flush diversion num %i in list: %i\n", divnum, i);
      }
    }
  }

  return(out);
}


int find_smallest_div(void)
{
  int i,
      min,
      found;

      found = 0;
      min = INT_MAX;

  for(i = 0; i < end_div_list; i++)
  {
    if(((*div_data)[i].divnum > 0) && ((*div_data)[i].divnum < min))
    {
      min = (*div_data)[i].divnum;
      found = 1;
    }
  }

  if(debug)
  {
    printf("found smallest diversion = %i", min);
  }
  
  if(found == 0)
  {
    min = -1;
  }

  if(debug)
  {
    printf(" returning %i\n", min);
  }
  
  return(min);
}


data_buffer *flush_all_diversions(data_buffer *out)
{
  int divnum;

  divnum = 1;

  while(divnum > 0)
  {
    divnum = find_smallest_div();

    if(divnum > 0)
    {
      out = flush_diversion(divnum, out);
    }
  }

  return(out);
}


void write_diversion(uint8_t *buf, int len, int divnum)
{
  int togo,
      room,
      div_list;

  togo = len;

  if(divnum > 0)
  {
    div_list = pos_div_list;
  }
  else
  {
    div_list = at_last_div_list;
  }    
    
  while(togo > 0)
  {
    room = div_entry_size - (*div_data)[div_list].length;

    if(room >= togo)
    {
      /* enough room in current entry */
      if(debug)
      {
        printf("Div Store enough len= %i, pos= %i\n", togo, (*div_data)[div_list].length);
      }
      memcpy(&((*div_data)[div_list].data[(*div_data)[div_list].length]), buf, togo);
      (*div_data)[div_list].length += togo;
      togo = 0;
    }
    else
    {
      /* not enough room in current entry */

      if(room > 0)
      {
        if(debug)
        {
          printf("Div Store NOT enough len= %i, pos= %i\n", room, (*div_data)[div_list].length);
        }
        memcpy(&((*div_data)[div_list].data[(*div_data)[div_list].length]), buf, room);
        (*div_data)[div_list].length += room;
        togo -= room;
        buf += room;
      }

      div_list = new_diversion_entry(divnum, div_list);

    }

  }

  if(divnum > 0)
  {
    pos_div_list = div_list;
  }
  else
  {
    at_last_div_list = div_list;
  }

}

void write_in_at_last(uint8_t *in, int len)
{
 
  if(at_last_div_list < 0)
  {
    open_diversion(0, 0);
  }
  
  write_diversion(in, len, 0);
  
}
  
void write_output(data_buffer *buf, int reserve)
{
  int written_bytes,
      bytes_to_write;
  int i, j;

  if(buf->position > reserve)
  {
    /* normally this function is called when the position is at the end of the buffer
     * here the amount to be written is calculated */

    bytes_to_write = buf->position - reserve;

    /* depending on diversion different actions are taken
     * if diversion num < 0 then no action and thus data is
     * not written and thereby lost
     */
    if(buf->divnum == 0)
    {
      written_bytes = write(buf->file, buf->data, bytes_to_write);

      if(debug)
      {
        printf("\n ----- written number of bytes: %i to %s -----\n",written_bytes, buf->filename);
      }
      
      if (written_bytes != bytes_to_write)
      {
        fprintf(stderr, "Error writing output file: %s: %s.\n", buf->filename, strerror(errno));
        exit(Exit_io);
      }
    }

    if(buf->divnum > 0)
    {
      write_diversion(buf->data, bytes_to_write, buf->divnum);
    }

    /* copy remaining bytes to the beginning of the buffer */
    j = 0;
    for(i = bytes_to_write; i < buf->size; i++)
    {
      buf->data[j] = buf->data[i];
      j++;
    }

    buf->position = reserve;
  }
}

void flush_output(data_buffer *output)
{
  
  if(output->file >= 0)
  {
    /* output to real file or stdout */
    write_output(output, 0);
  
  }
}



void open_trace(char *filename)
{
  /* open output */
  if(filename[0] == '-' && filename[1] == '\0')
  {
    /* stdout as output */
    trace_file = STDOUT_FILENO;
  }
  else
  {
    /* open file for output */
    trace_file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (trace_file < 0)
    {
      fprintf(stderr, "Error; can not open file for writing: %s: %s.\n", filename, strerror(errno));
      exit(Exit_io);
    }
  }
}


void close_trace(void)
{
  
  if(trace_file != STDOUT_FILENO) /* if stdout do not close */
  {
    /* close file */
    if(close(trace_file) < 0)
    {
      fprintf(stderr, "Error closing trace file: %s.\n", strerror(errno));
      exit(Exit_io);
    }
  }
}

void trace_line(int depth)
{
  int ret;
  
  ret = dprintf(trace_file, "\n%*cline: %4i, depth: %i:", depth, ' ', line_counter, depth);
    
  if (ret < 0)
  {
    fprintf(stderr, "Error writing trace file: %s.\n", strerror(errno));
    exit(Exit_io);
  }
  
}

void output_trace(uint8_t *in, int len)
{
  int written_bytes;
  
  written_bytes = write(trace_file, in, len);
  
  
  if (written_bytes != len)
  {
    fprintf(stderr, "Error writing trace file: %s.\n", strerror(errno));
    exit(Exit_io);
  }
  
}
