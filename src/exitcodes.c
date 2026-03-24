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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "exitcodes.h"
#include "input.h"

/* the code returned when exiting */
int exit_code;

/* warning level
 * 0: no warnings
 * 1: warnings, no exit code
 * 2: warnings and exit code
 * 3: warnings and always exit
 */
int warning_level = 1;


void print_warning(int exitcode, char *format, ...)
{
   va_list args;
   
   va_start(args, format);

   fprintf(stderr, "%s; Error line: %i in file: %s line: %i; ", program_name, line_counter, current_input_file_buffer->filename, local_line_counter);
   
   if(warning_level >= 1)
   {
     vfprintf(stderr, format, args);
   }

   if(warning_level >= 2)
   {
     exit_code = exitcode;
   }

   if(warning_level >= 3)
   {
     exit(exitcode);
   }
   
   va_end(args);
}
