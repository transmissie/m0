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

/* only extern because used for statistics */
extern int size_div_list,
           end_div_list;

/* var holds the current length of a possible vlm */
extern int vlm_reserve;
           
extern data_buffer *output_buffer;

void init_div_list(void);

void open_diversion(int, int);

void putchars_buffer(uint8_t *, int, data_buffer **);

void putchar_buffer(uint8_t, data_buffer **);

void flush_diversion(int, data_buffer **);

void flush_all_diversions(data_buffer **);

void write_in_at_last(uint8_t *, int);

void write_output(data_buffer **, int);

void flush_output(data_buffer **);

void open_trace(char *);

void trace_line(int);

void output_trace(uint8_t *, int);

void close_trace(void);
