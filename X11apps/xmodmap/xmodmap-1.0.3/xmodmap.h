/* $Xorg: xmodmap.h,v 1.4 2001/02/09 02:05:56 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xmodmap/xmodmap.h,v 1.4 2001/01/17 23:46:21 dawes Exp $ */

extern const char *ProgramName;
extern Display *dpy;
extern int min_keycode, max_keycode;
extern Bool verbose;
extern Bool dontExecute;
extern const char *inputFilename;
extern int lineno;
extern int parse_errors;

extern void initialize_map(void);
extern void process_file(const char *filename);
extern void process_line(char *buffer);
extern void handle_line(char *line, int len);
extern void print_work_queue(void);
extern int execute_work_queue(void);
extern void print_modifier_map(void);
extern void print_key_table(Bool exprs);
extern void print_pointer_map(void);

extern int UpdateModifierMapping(XModifierKeymap *map);
extern int AddModifier(XModifierKeymap **mapp, KeyCode keycode, int modifier);
extern int RemoveModifier(XModifierKeymap **mapp, KeyCode keycode, 
			  int modifier);
extern int ClearModifier(XModifierKeymap **mapp, int modifier);
extern void PrintModifierMapping(XModifierKeymap *map, FILE *fp);
extern void PrintKeyTable(Bool exprs, FILE *fp);
extern void PrintPointerMap(FILE *fp);
extern int SetPointerMap(unsigned char *map, int n);

extern void *chk_malloc(size_t n_bytes);
