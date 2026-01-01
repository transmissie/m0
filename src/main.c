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
#include <argp.h>

#include "definesizes.h"
#include "exitcodes.h"
#include "config.h"
#include "sds.h"
#include "input.h"
#include "output.h"
#include "processor.h"
#include "statistics.h"

#include <fcntl.h>
#include <unistd.h>


const char *argp_program_version = PACKAGE_STRING "\n Copyright (C) 2025 Marco de Beurs, Alex de Beurs\n License GPLv3: GNU General Public License version 3 <https://www.gnu.org/licenses/>\n"
"SDSlib:\n Copyright (C) 2006-2015, Salvatore Sanfilippo\n Copyright (C) 2015, Oran Agra, Redis Labs, Inc\n"
"\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.";

const char *argp_program_bug_address = PACKAGE_BUGREPORT;


/* Program documentation. */
static char doc[] =
  "Process macros in FILEs.  If no FILE or if FILE is `-', standard input is read. The output is standard output unless the option --output is used to output to a FILE.";

/* A description of the arguments we accept. */
static char args_doc[] = "[FILE...]";

/* The options we understand. */
static struct argp_option options[] = {
  // {"quiet",    'q', 0,      0,  "Don't produce any output", 0},
  {"statistics",   's', 0,      0,  "Print statistics of macros and internal memories at end of program. This is output to standard output.", 0},
  {"define",   'D', "name[=value]" ,      0,  "Define a macro; if value is missing, the value is an empty string. "
    "The value can be any string, but the macro can not be defined to take arguments."
    " The order with respect to file names is not significant.", 0},
  {"output",   'o', "FILE", 0,  "Output to FILE instead of standard output.", 0},
  {"traceon",  'T', 0,      0,  "Set tracing on. This will output information when a macro is called to standard output by default.", 0},
  {"tracefile",'t', "TRACEFILE", 0,  "Output trace information to TRACEFILE instead of standard output.", 0},
  { 0 }
};


/* Used by main to communicate with parse_opt. */
struct arguments
{
  sds args;                /* a list of all arguments */
  sds var_options;         /* a list of all options after the -- */ 
  sds defines;             /* a list of all macro defines */ 
  int number_args,
      silent,
      statistics,
      verbose,
      trace;
  char *output_file;
  char *trace_file;
};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
   *     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;
  
  switch (key)
  {
    case 'q':
      arguments->silent = 1;
      // printf("q ");
      break;
    case 's':
      arguments->statistics = 1;
      // printf("s ");
      break;
    case 'D':
      // printf("D: %s", arg);
      arguments->defines = sdscat(arguments->defines, arg);
      arguments->defines = sdscat(arguments->defines, "\n");
      break;
    case 'v':
      arguments->verbose = 1;
      // printf("v ");
      break;
    case 'T':
      arguments->trace = 1;
      // printf("T ");
      break;
    case 'o':
      arguments->output_file = arg;
      // printf("o: %s ",arg);
      break;
    case 't':
      arguments->trace_file = arg;
      // printf("d: %s ",arg);
      break;
      
    case ARGP_KEY_ARG:
      if(arg[0] == '-' && arg[1] != '\x00')
      {
        /* this is an option after the -- */
        arguments->var_options = sdscat(arguments->var_options, arg);
        arguments->var_options = sdscat(arguments->var_options, "\n");
        // printf("key arg: %s ",arg);
        
      }
      else
      {
        arguments->args = sdscat(arguments->args, arg);
        arguments->args = sdscat(arguments->args, "\n");
        // printf("%s ",arg);
        arguments->number_args++;
      }
      
      break;
      
      
    case ARGP_KEY_END:
      if (arguments->number_args < 1)
      {
        /* No arguments, thus use default input */
        arguments->args = sdscat(arguments->args, "-");
        arguments->number_args = 1;
      }
      break;
      
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}



char *next_arg(char *args, int *current_arg_index, int *next_arg_index)
{
  
  *current_arg_index = *next_arg_index; /* save the next_arg_index, this will become the index for the current argument */
  
  /* find end of argument in args string */
  while(args[*next_arg_index] != '\n' && args[*next_arg_index] != '\0')
  {
    (*next_arg_index)++;
  }
  
  /* check for end of argument instead of end of string */
  if(args[*next_arg_index] == '\n')
  {
    args[*next_arg_index] = 0; /* make the string end here */
    (*next_arg_index)++; /* the next argument start is one char further */
  }

  return(&args[*current_arg_index]);
}



/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc, NULL, NULL, NULL};

struct arguments arguments;


int main (int argc, char **argv)
{

  char *input,
       *definition;
  data_buffer *file_input,
              *file_output;
  int  current_arg_index = 0,
       next_arg_index = 0;
  int  current_def_index = 0,
       next_def_index = 0;

  exit_code = Exit_OK;
  
  /* Default values. */
  arguments.args = sdsempty();
  arguments.var_options = sdsempty();
  arguments.defines = sdsempty();
  arguments.number_args = 0;
  arguments.silent = 0;
  arguments.statistics = 0;
  arguments.verbose = 0;
  arguments.trace = 0;
  arguments.output_file = "-";
  arguments.trace_file = "-";

  /* set program name used by macros */
  program_name = argv[0];

  /* Parse our arguments; every option seen by parse_opt will be reflected in arguments. */
  argp_parse (&argp, argc, argv, ARGP_IN_ORDER, 0, &arguments);

  if(debug)
  {
    printf ("ARGs = %s\n number of args %i\n var_options = %s\n defines = %s\n OUTPUT_FILE = %s\n"
          "TRACE_FILE = %s\n VERBOSE = %s\nSILENT = %s\n",
          arguments.args,
          arguments.number_args,
          arguments.var_options,
          arguments.defines,
          arguments.output_file,
          arguments.trace_file,
          arguments.verbose ? "yes" : "no",
          arguments.silent ? "yes" : "no");
  }
  
  /* set options after --,  used by macros */
  arg_options = arguments.var_options;;

  /* the first input buffer, used for the files in the arguments */ 
  file_input = alloc_io_buffer(input_buffer_size);
  
  /* the buffer for the output */ 
  file_output = alloc_io_buffer(output_buffer_size);

  /* open trace file */
  if(arguments.trace != 0)
  {
    trace = Trace_on;
    open_trace(arguments.trace_file);
  }
  else
  {
    trace = Trace_off;
  }
  
  init_process(arguments.output_file, file_output); /* initialise data for processor and open output */

  /* process the defines from the command line */
  definition = next_arg(arguments.defines, &current_def_index, &next_def_index);
  while(definition[0] != '\0')
  {
    init_definition(definition);
    definition = next_arg(arguments.defines, &current_def_index, &next_def_index);
  }  
    
  /* get the first input (file) */
  input = next_arg(arguments.args, &current_arg_index, &next_arg_index);

  while(input[0] != '\0')
  {
    if(debug)
    {
      printf(" next file: %s\n", input);
    }
    /* open the input file */
    open_input(input, file_input);
    current_input_file_buffer = file_input;
    
    /* read the first input to fill the buffer and start processing */
    read_input(file_input, 0);
    
    if(debug)
    {
      printf("\n\n ------------- \n size: %i\n -------------\n", file_input->length);
    }
    
    process_input(file_input, &file_output, Run_macro_yes);
      
    
    close_input(file_input);
    
    /* get the next input (file) */
    input = next_arg(arguments.args, &current_arg_index, &next_arg_index);
    
  }
 
  close_process(&file_output);

  if(debug_statistics || (arguments.statistics == 1))
  {
    print_statistics();
  }

  /* close trace file */
  if(arguments.trace != 0)
  {
    close_trace();
  }
  
  return(exit_code);
}
