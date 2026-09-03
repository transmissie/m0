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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <argp.h>

#include "definesizes.h"
#include "xmalloc.h"
#include "exitcodes.h"
#include "config.h"
#include "sds.h"
#include "input.h"
#include "output.h"
#include "processor.h"
#include "statistics.h"

#include <fcntl.h>
#include <unistd.h>


const char *argp_program_version = PACKAGE_STRING "\n Copyright (C) 2025, 2026 Marco de Beurs, Alex de Beurs\n"
"License GPLv3: GNU General Public License version 3 <https://www.gnu.org/licenses/>\n"
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
  {"statistics",   's', 0,      0,  "Print statistics of macros and internal memories at end of program. This is output to standard output.", 0},
  {"define",   'D', "name[=value]" ,      0,  "Define a macro; if value is missing, the value is an empty string. "
    "The value can be any string, but the macro can not be defined to take arguments."
    " The order with respect to file names is not significant.", 0},
  {"fatal-warnings",   'E', 0, 0,  "Controls the effect of warnings. "
    "If specified once, warnings are printed, exit codes are not set and execution continues. "
    "If specified twice, warnings are printed, exit codes are set, but execution continues. "
    "If specified three times, warnings are printed, exit codes are set, execution stops.", 0},
  {"output",   'o', "FILE", 0,  "Output to FILE instead of standard output.", 0},
  {"ordered-options",   'O', 0, 0,  "After setting this, the options on the command line are valid for succeeding files."},
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
      statistics,
      order,
      trace,
      warning_level;
  char *output_file;
  char *trace_file;
};


struct ordered_options
{
  int  order,
       trace,
       warning_level,
       output_change,
       tracefile_change;
  sds var_options;         /* a list of options after the -- before the input file */ 
  sds defines;             /* a list of all macro defines if ordered options */ 
  char *output_file;
  char *trace_file;
};

struct ordered_options *ordered_opt;
int size_ordered_opt = 0;


void set_ordered_options(struct arguments *arguments)
{
  if(ordered_opt == NULL)
  {
    ordered_opt = xmalloc(sizeof(struct ordered_options) * size_init_ordered_opt);
    size_ordered_opt = size_init_ordered_opt;    
  }
  
  if(arguments->number_args >= size_ordered_opt)
  {
    ordered_opt = xrealloc(ordered_opt, sizeof(struct ordered_options) * (size_ordered_opt + size_add_ordered_opt));
    size_ordered_opt += size_add_ordered_opt;    
  }
    
    
  ordered_opt[arguments->number_args].var_options = sdsempty();
  ordered_opt[arguments->number_args].defines = sdsempty();
  ordered_opt[arguments->number_args].order = arguments->order;
  ordered_opt[arguments->number_args].trace = arguments->trace;
  ordered_opt[arguments->number_args].warning_level = arguments->warning_level;
  ordered_opt[arguments->number_args].output_file = NULL;
  ordered_opt[arguments->number_args].output_change = 0;
  ordered_opt[arguments->number_args].trace_file = NULL;
  ordered_opt[arguments->number_args].tracefile_change = 0;
  
}



/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
   *     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;
  
  switch (key)
  {
    case 's':
      arguments->statistics = 1;
      // printf("s ");
      break;
    case 'D':
      // printf("D: %s", arg);
      if(arguments->order == 0)
      {
        arguments->defines = sdscat(arguments->defines, arg);
        arguments->defines = sdscat(arguments->defines, "\n");
      }
      else
      {
        ordered_opt[arguments->number_args].defines = sdscat(ordered_opt[arguments->number_args].defines, arg);
        ordered_opt[arguments->number_args].defines = sdscat(ordered_opt[arguments->number_args].defines, "\n");
      }
      break;
    case 'E':
      // printf("E: %s", arg);
      if(arguments->order == 0)
      {
        arguments->warning_level++;
      }
      else
      {
        ordered_opt[arguments->number_args].warning_level++;
      }
      break;
    case 'O':
      arguments->order = 1;
      ordered_opt[arguments->number_args].order = 1;
      // printf("v ");
      break;
    case 'T':
      if(arguments->order == 0)
      {
        arguments->trace = 1;
      }
      else
      {
        ordered_opt[arguments->number_args].trace = 1;
      }
        
      // printf("T ");
      break;
    case 'o':
      if(arguments->order == 0)
      {
        arguments->output_file = arg;
      }
      else
      {
        ordered_opt[arguments->number_args].output_file = arg;
        ordered_opt[arguments->number_args].output_change = 1;
      }
      // printf("o: %s ",arg);
      break;
    case 't':
      if(arguments->order == 0)
      {
        arguments->trace_file = arg;
      }
      else
      {
        ordered_opt[arguments->number_args].trace_file = arg;
        ordered_opt[arguments->number_args].tracefile_change = 1;
      }
      // printf("d: %s ",arg);
      break;
      
    case ARGP_KEY_ARG:
      if(arg[0] == '-' && arg[1] != '\x00')
      {
        /* this is an option after the -- */
        arguments->var_options = sdscat(arguments->var_options, arg);
        arguments->var_options = sdscat(arguments->var_options, "\n");

        ordered_opt[arguments->number_args].var_options = sdscat(ordered_opt[arguments->number_args].var_options, arg);
        ordered_opt[arguments->number_args].var_options = sdscat(ordered_opt[arguments->number_args].var_options, "\n");

        // printf("key arg: %s, num: %i\n",arg, arguments->number_args);
        
      }
      else
      {
        arguments->args = sdscat(arguments->args, arg);
        arguments->args = sdscat(arguments->args, "\n");
        // printf("%s at num: %i\n",arg, arguments->number_args);
        
        arguments->number_args++;
        set_ordered_options(arguments);
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
  int  current_file_index = 0;

  exit_code = Exit_OK;
  
  /* Default values. */
  arguments.args = sdsempty();
  arguments.var_options = sdsempty();
  arguments.defines = sdsempty();
  arguments.number_args = 0;
  arguments.statistics = 0;
  arguments.order = 0;
  arguments.trace = 0;
  arguments.warning_level = 0;
  arguments.output_file = "-";
  arguments.trace_file = "-";

  set_ordered_options(&arguments);
  
  /* set program name used by macros */
  program_name = argv[0];

  /* Parse our arguments; every option seen by parse_opt will be reflected in arguments. */
  argp_parse (&argp, argc, argv, ARGP_IN_ORDER, 0, &arguments);

  if(debug)
  {
    printf ("ARGs = %s\n number of args %i\n var_options = %s\n defines = %s\n OUTPUT_FILE = %s\n"
          "TRACE_FILE = %s\n ORDER = %s\n",
          arguments.args,
          arguments.number_args,
          arguments.var_options,
          arguments.defines,
          arguments.output_file,
          arguments.trace_file,
          arguments.order ? "yes" : "no");
  }
  
  /* set options after --,  used by macros */
  arg_options = arguments.var_options;;

  /* set warning level */
  warning_level = arguments.warning_level;
  
  /* the first input buffer, used for the files in the arguments */ 
  file_input = alloc_io_buffer(input_buffer_size);
  
  /* the buffer for the output */ 
  file_output = alloc_io_buffer(output_buffer_size);

  /* open trace file */
  open_trace(arguments.trace_file);

  if(arguments.trace != 0)
  {
    trace = Trace_on;
  }
  else
  {
    trace = Trace_off;
  }
  
  open_output(arguments.output_file, file_output);
  // init_process(arguments.output_file, file_output); /* initialise data for processor and open output */
  init_process(); /* initialise data for processor */

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

    // printf ("input: %s  index:%i   var_options = %s\n defines = %s\n OUTPUT_FILE = %s\n"
    //       "TRACE_FILE = %s\n ORDER = %s\n", input, current_file_index,
    //       ordered_opt[current_file_index].var_options,
    //       ordered_opt[current_file_index].defines,
    //       ordered_opt[current_file_index].output_file,
    //       ordered_opt[current_file_index].trace_file,
    //       ordered_opt[current_file_index].order ? "yes" : "no");
    /* carry out the ordered options if set */
    arg_options_local = ordered_opt[current_file_index].var_options;
    
    if(ordered_opt[current_file_index].order > 0)
    {
       warning_level = ordered_opt[current_file_index].warning_level;
      
       /* change trace file */
       if(ordered_opt[current_file_index].tracefile_change != 0)
       {
         close_trace();
         open_trace(ordered_opt[current_file_index].trace_file);
       }

       /* set tracing */
       if(ordered_opt[current_file_index].trace != 0)
       {
         trace = Trace_on;
       }
       else
       {
         trace = Trace_off;
       }

       /* process the local defines */
       current_def_index = 0,
       next_def_index = 0;
       definition = next_arg(ordered_opt[current_file_index].defines, &current_def_index, &next_def_index);
       while(definition[0] != '\0')
       {
         init_definition(definition);
         definition = next_arg(ordered_opt[current_file_index].defines, &current_def_index, &next_def_index);
       }
       
       /* change output file */
       if(ordered_opt[current_file_index].output_change != 0)
       {
         close_output(&file_output);
         open_output(ordered_opt[current_file_index].output_file, file_output);
       }
    }
    
    /* at first input */
    process_atfirst(&file_output);
    
    if(debug)
    {
      printf(" next file: %s\n", input);
    }
    /* open the input file */
    open_input(input, file_input);
    current_input_file_buffer = file_input;
    
    /* read the first input to fill the buffer and start processing */
    read_input(&file_input, 0);
    
    if(debug)
    {
      printf("\n\n ------------- \n size: %i\n -------------\n", file_input->length);
    }
    
    process_input(&file_input, &file_output, Run_macro_yes);
      
    
    close_input(file_input);
    
    /* get the next input (file) */
    input = next_arg(arguments.args, &current_arg_index, &next_arg_index);
    current_file_index++;
  }
 
  close_process(&file_output);
  close_output(&file_output);

  if(debug_statistics || (arguments.statistics == 1))
  {
    print_statistics();
  }

  /* close trace file */
  // if(arguments.trace != 0)
  {
    close_trace();
  }
  
  return(exit_code);
}
