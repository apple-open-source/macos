/* Copyright (C) 1997 Klee Dienes <klee@mit.edu>.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "dis-asm.h"

static char *oscs[] = {
  "LP", "XT", "HS", "RC"
};

static char *pic12_z00_instr_names[] = {
  "???", "clrf", "subwf", "decf",
  "iorwf", "andwf", "xorwf", "addwf",
  "movf", "comf", "incf", "decfsz",
  "rrf", "rlf", "swapf", "incfsz"
};

static char *pic12_z01_instr_names[] = {
  "bcf", "bsf", "btfsc", "btfss"
};

static char *pic12_z11_instr_names[] = {
  "movlw", "iorlw", "andlw", "xorlw"
};

static char *pic14_z00_instr_names[] = {
  "???", "clrf", "subwf", "decf",
  "iorwf", "andwf", "xorwf", "addwf",
  "movf", "comf", "incf", "decfsz",
  "rrf", "rlf", "swapf", "incfsz"
};

static char *pic14_z01_instr_names[] = {
  "bcf", "bsf", "btfsc", "btfss"
};

static char *pic14_z11_instr_names[] = {
  "movlw", "movlw", "movlw", "movlw",
  "retlw", "retlw", "retlw", "retlw",
  "iorlw", "andlw", "xorlw", "???",
  "sublw", "sublw", "addlw", "addlw"
};

int print_insn_pic16 (bfd_vma addr, struct disassemble_info *info)
{
  void *stream = info->stream;
  fprintf (stream, "???");
  return 2;
}

int print_insn_pic14 (bfd_vma addr, struct disassemble_info *info)
{
  fprintf_ftype fprintf = info->fprintf_func;
  void *stream = info->stream;
  unsigned char buf[2];
  unsigned int i;
  int status;
  
  status = info->read_memory_func (addr, buf, 2, info);
  if (status != 0) {
    info->memory_error_func (status, addr, info);
    return -1;
  }
  i= (((unsigned int) buf[1]) << 8) + (((unsigned int) buf[0]) << 0);

  if ((addr >= 0x4000) && (addr < 0x4008)) {
    i &= 0x7f;
    if (i == 0x7f || i < 0x20) {
      fprintf (stream, "ID");
    } else {
      fprintf (stream, "ID '%c'", i);
    }
    return 2;
  }

  if (addr == 0x400e) {
    fprintf (stream, "config CP=%s, PWRTE=%s, WDTE=%s, OSC=%s",
	     (i & 0x10 ? "off" : "on"),
	     (i & 0x8 ? "on" : "off"),
	     (i & 0x4 ? "on" : "off"),
	     oscs[i & 0x3]);
    return 2;
  }

  switch (i >> 12) {
  case 0:
    if ((i & 0x3f00) == 0) {
      if (i & 0x80) {
	fprintf (stream, "movwf %02x", i & 0x7f);
	return 2;
      } else {
	switch (i) {
	case 0x00:
	case 0x20:
	case 0x40:
	case 0x60:
	  fprintf (stream, "nop");
	  return 2;

	case 0x08:
	  fprintf (stream, "return");
	  return 2;

	case 0x09:
	  fprintf (stream, "retfie");
	  return 2;

	case 0x62:
	  fprintf (stream, "option");
	  return 2;

	case 0x63:
	  fprintf (stream, "sleep");
	  return 2;

	case 0x64:
	  fprintf (stream, "clrwdt");
	  return 2;

	case 0x65:
	case 0x66:
	case 0x67:
	  fprintf (stream, "tris %02x", i & 7);
	  return 2;

	default:
	  fprintf (stream, "???");
	  return 2;
	}
      }
    } else {
      if ((i & 0x3f80) == 0x100) {
	fprintf (stream, "clrw");
	return 2;
      } else {
	fprintf (stream, "%s %02x,%s", pic14_z00_instr_names[(i >> 8) & 0xf], i & 0x7f, (i & 0x80 ? "f" : "w"));
	return 2;
      }
    }
    break;

  case 1:
    fprintf (stream, "%s %02x,%d", pic14_z01_instr_names[(i >> 10) & 3], i & 0x7f, (i >> 7) & 7);
    return 2;
    
  case 2:
    fprintf (stream, "%s %04x", (i & 0x800 ? "goto" : "call"), i & 0x7ff);
    return 2;
    
  case 3:
    fprintf (stream, "%s %02x", pic14_z11_instr_names[(i >> 8) & 0xf], i & 0xff);
    return 2;
  }
  
  return 2;
}

int print_insn_pic12 (bfd_vma addr, struct disassemble_info *info)
{
  fprintf_ftype fprintf = info->fprintf_func;
  void *stream = info->stream;
  unsigned char buf[2];
  unsigned int i;
  int status;
  
  status = info->read_memory_func (addr, buf, 2, info);
  if (status != 0) {
    info->memory_error_func (status, addr, info);
    return -1;
  }
  i= (((unsigned int) buf[1]) << 8) + (((unsigned int) buf[0]) << 0);

  
  if (addr == 0xfff) {
    fprintf (stream, "config CP=%s, WDTE=%s, OSC=%s",
	     (i & 0x8 ? "off" : "on"),
	     (i & 0x4 ? "on" : "off"),
	     oscs[i & 0x3]);
    return 2;
  }

  switch (i >> 10) {
	  case 0:
	    if((i & 0xfc0) == 0)
	      {
		if(i & 0x20)
		  fprintf (stream, "movwf %02x", i & 0x1f);
		else
		  switch(i)
		    {
		      case 0x00:
		        fprintf (stream, "nop");
		        return 2;

		      case 0x02:
			fprintf (stream, "option");
			return 2;

		      case 0x03:
			fprintf (stream, "sleep");
			return 2;

		      case 0x04:
			fprintf (stream, "clrwdt");
			return 2;

		      case 0x05: case 0x06: case 0x07:
			fprintf (stream, "tris %02x", i & 7);
			return 2;

		      default:
			fprintf (stream, "???");
			return 2;
		    }
	      }
	    else
	      {
		if(i == 0x040)
		  fprintf (stream, "clrw");
		else
		  fprintf (stream, "%s %02x,%s",
			 pic12_z00_instr_names[(i >> 6) & 0xf],
			 i & 0x1f, (i & 0x20 ? "f" : "w"));
	      }
	    return 2;

	  case 1:
	    fprintf (stream, "%s %02x,%d",
		   pic12_z01_instr_names[(i >> 8) & 3], i & 0x1f, (i >> 5) & 7);
	    return 2;

	  case 2:
	    if(i & 0x200)
	      fprintf (stream, "goto %04x", i & 0x1ff);
	    else if(i & 0x100)
	      fprintf (stream, "call %04x", i & 0xff);
	    else
	      fprintf (stream, "retlw %02x", i & 0xff);
	    return 2;

	  case 3:
	    fprintf (stream, "%s %02x",
		   pic12_z11_instr_names[(i >> 8) & 0x3],
		   i & 0xff);
	    return 2;
	}

  fprintf (stream, "???");
  return 2;
}
