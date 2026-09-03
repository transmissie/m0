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


typedef enum
{
  Run_macro_no,
  Run_macro_yes
} run_macro;

typedef enum
{
  Run_fill_in_no,
  Run_fill_in_yes
} fill_in;

typedef enum
{
  Trace_off,
  Trace_on
} trace_setting;

extern trace_setting trace;

void init_definition(char *def);

void init_process(/*char *, data_buffer **/ void);

void process_atfirst(data_buffer **);

void close_process(data_buffer **);

void process_input(data_buffer **, data_buffer **, run_macro);
