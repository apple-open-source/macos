/*--------------------------------------------------------------------------------------*
 |                                                                                      |
 |                                    gdb_testing.i                                     |
 |                                                                                      |
 |            Separate place to "bread board", test, and expirement with gdb            |
 |                                                                                      |
 |                                     Ira L. Ruben                                     |
 |                       Copyright Apple Computer, Inc. 2000-2006                       |
 |                                                                                      |
 *--------------------------------------------------------------------------------------*

 This file is just for "fooling around".  It is included as part of gdb.c and uses its
 #include setup.  As such it can talk directly to gdb since the includes and -I's for
 gdb.c allow using its frameworks. 
*/

/*--------------------------------------------------------------------------------------*/

static char *(*saved_cmd_line_input_hook)(char *, int, char *) = NULL;
static void (*saved_window_hook)(FILE *, char *) = NULL;
extern void (*window_hook) PARAMS ((FILE *, char *));

/*--------------------------------------------------------------------------------------*/

static char rawline[1024];
static int cp = 0;

static void (*saved_rl_getc_function)();
static int first_testx = 1;

static int my_getc(FILE *stream)
{
    int i, c = ((int (*)(FILE*))saved_rl_getc_function)(stream);
    rawline[cp++] = c;
    
    //fprintf(stderr, "%.2x\n", c);
    
    if (c == '\n' || c == '\r') {
    	rawline[cp] = 0;
	fprintf(stderr, "\n");
    	for (i = 0; i < cp; ++i)
	    fprintf(stderr, "%.2x ", rawline[i] & 0xFF);
	fprintf(stderr, " ==> ");
    	for (i = 0; i < cp; ++i) {
	    c = rawline[i] & 0xFF;
	    fprintf(stderr, "%c", (c < ' ') ? '¥' : c);
	}
	fprintf(stderr, "\n");
	cp = 0;
    }
    
    return (c);
}

void gdb_testx(char *arg, int from_tty)
{
    if (first_testx) {
    	first_testx  = 0;
    	saved_rl_getc_function = rl_getc_function;
    	rl_getc_function = (void (*)())my_getc;
    }
    cp = 0;
}

/*--------------------------------------------------------------------------------------*/

void gdb_testy(char *arg, int from_tty)
{
}

/*--------------------------------------------------------------------------------------*/

static Gdb_Cmd_Class myClass = Gdb_Private;

void a_cmd(char *arg, int from_tty) 
{
    gdb_printf("a_cmd(%s)\n", a_cmd ? a_cmd : "<null>");
}

void gdb_testa(char *arg, int from_tty)
{
    struct cmd_list_element *c;
    char   alias[1024];
    
    if (arg && *arg) {
    	gdb_define_cmd(arg, a_cmd, myClass, "cmd help.");
	
	if (strlen(arg) > 1) {
	    strcpy(alias, arg);
	    alias[0]= 'X';
	    gdb_define_cmd_alias(arg, alias);
	}
    }
}

/*--------------------------------------------------------------------------------------*/

//#include "ui-file.h"
void gdb_testw(char *addr)
{
  struct objfile *objfile;
  struct obj_section *p;
  
  #if 0
  if (addr && *addr) {
      p = find_pc_section((CORE_ADDR)gdb_get_address(addr));
      if (p)
	  gdb_printf("%s is %s\n", bfd_get_filename(p->objfile->obfd), bfd_section_name(unused, p->the_bfd_section));
  }
  #else
  ALL_OBJFILES(objfile) {
      #if 1
      print_section_info_objfile(objfile);
      gdb_printf("-------------------------------\n");
      #elif 0
      ui_out_field_string(uiout, "filename", bfd_get_filename(objfile->obfd));
      ui_out_text (uiout, "\n");
      gdb_printf("-------------------------------\n");
      #elif 0
      print_section_info_objfile(objfile);
      for (p = objfile->sections; p < objfile->sections_end; p++) {
      	if (find_pc_sect_section((CORE_ADDR)addr, p)) {
      	    gdb_printf("%s\n", bfd_get_filename(objfile->obfd));
      	    break;
      	}
      }
      #endif
  }
  #endif
}

/*--------------------------------------------------------------------------------------*/

void gdb_testz(char *arg, int from_tty)
{
    #if 0
    int regnum;
    //int numregs = ARCH_NUM_REGS;
    char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
    char raw_buffer[MAX_REGISTER_RAW_SIZE];
    
    if (!arg || !*arg)
    	return;
	
    regnum = frame_map_name_to_regnum (arg, 2);
    if (regnum < 0)
	gdb_error("undefined");
	
    if (REGISTER_NAME(regnum) == NULL || *(REGISTER_NAME(regnum)) == '\0')
	gdb_error("undefined for machine");
    
    if (frame_register_read(selected_frame, regnum, raw_buffer))
	gdb_error("value not available");

    if (REGISTER_CONVERTIBLE(regnum)) {
	REGISTER_CONVERT_TO_VIRTUAL(regnum, REGISTER_VIRTUAL_TYPE(regnum),
					raw_buffer, virtual_buffer);
    } else
	memcpy(virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE(regnum));
    #endif
}

/*--------------------------------------------------------------------------------------*/

#if 0
#include <time.h>
#include <locale.h>
#include "breakpoint.h"
#include "ui-file.h"

#define ALL_BREAKPOINTS(B)  for (B = breakpoint_chain; B; B = B->next)
extern struct breakpoint *breakpoint_chain;

static void
write_one_breakpoint (struct breakpoint *b, FILE *stream)
{
  register struct command_line *l;
  
  switch (b->type)
    {
    case bp_watchpoint:
      fprintf_unfiltered (stream, "watch %s", b->exp_string);
      break;
    
    case bp_hardware_watchpoint:
      fprintf_unfiltered (stream, "watch %s", b->exp_string);
      break;
    
    case bp_read_watchpoint:
      fprintf_unfiltered (stream, "rwatch %s", b->exp_string);
      break;
    
    case bp_access_watchpoint:
      fprintf_unfiltered (stream, "awatch %s", b->exp_string);
      break;
          
    case bp_catch_load:
    case bp_catch_unload:
      fprintf_unfiltered (stream, "%scatch %sload", b->disposition == del ? "t" : "",
                                         b->type == bp_catch_unload ? "un" : "");
      if (b->dll_pathname != NULL)
        fputs_unfiltered (b->dll_pathname, stream);
      break;
      
    case bp_catch_fork:
      fprintf_unfiltered (stream, "%scatch fork", b->disposition == del ? "t" : "");
      break;
        
    case bp_catch_vfork:
      fprintf_unfiltered (stream, "%scatch vfork", b->disposition == del ? "t" : "");
      break;
      
    case bp_catch_exec:
      fprintf_unfiltered (stream, "%scatch exec", b->disposition == del ? "t" : "");
      break;

    case bp_catch_catch:
      fprintf_unfiltered (stream, "%scatch catch", b->disposition == del ? "t" : "");
      break;

    case bp_catch_throw:
      fprintf_unfiltered (stream, "%scatch throw", b->disposition == del ? "t" : "");
      break;
      
    case bp_breakpoint:
    case bp_hardware_breakpoint:
      if (b->enable == shlib_disabled)
        fputs_unfiltered ("future-", stream);
      fprintf_unfiltered (stream, "%s%sbreak", b->disposition == del ? "t" : "",
                        (b->type == bp_hardware_breakpoint) ? "h" : "");
      
      if (b->addr_string)
        {
          int len = strlen(b->addr_string) - 1;
	  if (b->addr_string[len] == ' ')
	    b->addr_string[len] = 0;
          else
            len = 0;
          fprintf_unfiltered (stream, " %s", b->addr_string);
          if (len)
	    b->addr_string[len] = ' ';
	}
      else if (b->source_file)
          fprintf_unfiltered (stream, " %s:%d", b->source_file, b->line_number);
      else
        fprintf_unfiltered(stream, " %s", 
		           local_hex_string_custom((unsigned long) b->address, "08l"));
      break;
    }
  
  if (b->thread != -1)
    fprintf_unfiltered (stream, " thread %d", b->thread);
  
  if (b->cond_string)
    fprintf_unfiltered (stream, " if %s", b->cond_string);
  
  fputc_unfiltered ('\n', stream);
  
  if ((l = b->commands))
    {
      fputs_unfiltered ("commands\n", stream);
      
      while (l)
        {
          print_command_line (l, 1, stream);
          l = l->next;
        }
      
      fputs_unfiltered ("end\n", stream);
    }
 
  if (b->ignore_count)
    fprintf_unfiltered (stream, "ignore $bpnum %d\n", b->ignore_count);
    
  if (b->enable == disabled)
      fputs_unfiltered ("disable $bpnum\n", stream);
}

void save_breakpoints_commandX (char *arg, int from_tty)
{
  char *pathname, buf[256], *p;
  register struct breakpoint *b;
  int found_a_breakpoint = 0, current_radix, skip, prev_radix;
  long n;
  FILE *fp;
  struct ui_file *stream;
  time_t t;
  
  extern char *tilde_expand (char *);
 
  if (!arg || !*arg)
    error ("Argument required (file name in which to save breakpoints");
  
  ALL_BREAKPOINTS (b)
    {
      /* Filter out non-user breakpoints. */
      if (   b->type != bp_breakpoint
          && b->type != bp_catch_load
          && b->type != bp_catch_unload
          && b->type != bp_catch_fork
          && b->type != bp_catch_vfork
          && b->type != bp_catch_exec
          && b->type != bp_catch_catch
          && b->type != bp_catch_throw
          && b->type != bp_hardware_breakpoint
          && b->type != bp_watchpoint
          && b->type != bp_read_watchpoint
          && b->type != bp_access_watchpoint
          && b->type != bp_hardware_watchpoint)
        continue;
      
      if (!found_a_breakpoint++)
        {
          if ((fp = fopen ((pathname = tilde_expand (arg)), "w")) == NULL)
            error ("Unable to open file '%s' for saving breakpoints (%s)",
                    arg, strerror (errno));
          stream = stdio_fileopen (fp);
          if (time (&t) != -1)
            {
              char *l= setlocale (LC_ALL, NULL);
              if (l)
                {
                  char *orig_locale = strcpy(xmalloc(strlen(l)+1), l);
                  (void)setlocale(LC_ALL, "");
                  if (strftime (buf, sizeof(buf), "%a %b %e %H:%M:%S %Z %Y", localtime (&t)))
                    fprintf_unfiltered (stream, "# Saved breakpoints file created on %s\n\n", buf);
                  setlocale(LC_ALL, orig_locale);
                }
            }
	  current_radix = -1;
        }

      skip = (b->commands || b->ignore_count || b->enable == disabled);
      if (skip)
        fputc_unfiltered ('\n', stream);

      if (b->input_radix != current_radix)
        {
          if (!skip && current_radix != -1)
            fputc_unfiltered ('\n', stream);
          // ------- determine initial radix here -------
          prev_radix = (current_radix == -1) ? input_radix : current_radix;
          if (prev_radix > 36)
            {
              ui_file_delete (stream);
              fclose (fp);
              remove (pathname);
              xfree (pathname);
              error ("Current radix (%d) for breakpoint #%d to large to support for saving.",
                     prev_radix, b->number);
            }
          n = b->input_radix;
          p = buf + 255;
          *p-- = '\0';
          do
            {
              *p-- = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[n % prev_radix];
              n /= prev_radix;
            } while (n);
          fprintf_unfiltered (stream, "set input-radix %s\n", ++p);
          current_radix = b->input_radix;
        }
        
      write_one_breakpoint (b, stream);
  
      if (skip && b->next)
        fputc_unfiltered ('\n', stream);
    }
  
  if (!found_a_breakpoint)
    printf_filtered ("No breakpoints or watchpoints to save.\n");
  else
    {
      // ------- restore original input radix here -------
      //fprintf_unfiltered (stream, "\nset input-radix %d\n", ???????);
      ui_file_delete (stream);
      fclose (fp);
      xfree (pathname);
      if (from_tty)
        printf_filtered ("Breakpoints saved to file '%s'.\n", arg);
    }
}
#endif

/*--------------------------------------------------------------------------------------*/

char *gdb_teste(GDB_ADDRESS addr)
{
    int  unmapped = 0;
    int  offset = 0;
    int  line = 0;
    char *filename = NULL;
    char *name = NULL;
    asection *section = NULL;
    struct minimal_symbol *msymbol;
    struct obj_section *addr_section;
    struct bfd_section *the_bfd_section;
    struct objfile *objfile;
    bfd *abfd;

    // references are bfd/bdd-in2.h, objfiles.h
    
    static char result[2048];
    
    #if 0
    if (!build_address_symbolic(addr, 0, &name, &offset, &filename, &line, &unmapped))
    	return (name);
    #endif
    
    msymbol = lookup_solib_trampoline_symbol_by_pc(addr);
    *result = '\0';
    if (msymbol)
    	strcpy(result, SYMBOL_LINKAGE_NAME(msymbol));
    
    #if 0
    msymbol = lookup_minimal_symbol_by_pc_section(addr, section);
    if (msymbol)
    	if (*result)
    	    strcat(result, " ");
    	sprintf(result + strlen(result), "(%s)",  SYMBOL_BFD_SECTION(msymbol)->name);
    #endif
    
    // LC_SEGMENT.segname.sectname
    // LC_SEGMENT.__TEXT.__cstring
    // LC_SEGMENT.__DATA.__cfstring
    // LC_SEGMENT.__OBJC.__cstring_object
    
    addr_section = find_pc_section(addr);
    if (addr_section) {
    	objfile = addr_section->objfile;
    	abfd = addr_section->objfile->obfd;
    	the_bfd_section = addr_section->the_bfd_section;
    	
    	if (the_bfd_section) {
    	    if (*result)
    	    	strcat(result, " ");
    	    sprintf(result + strlen(result), "(%s)",  the_bfd_section->name);
    	}
    
	if (bfd_get_section_by_name(abfd, "LC_SEGMENT.__DATA.__basicstring")) {
	    if (*result)
		strcat(result, " ");
	    sprintf(result + strlen(result), "(RealBasic)");
	}
	
	if (bfd_get_section_by_name(abfd, "LC_SEGMENT.__OBJC")) {
	    if (*result)
		strcat(result, " ");
	    sprintf(result + strlen(result), "(objc)");
	}
    }
    
    if (!*result)
    	*result = '\0';
    
    return (result);
}

/*--------------------------------------------------------------------------------------*/
