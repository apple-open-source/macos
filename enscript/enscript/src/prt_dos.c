/*
 * Printer interface for DOS.
 * Copyright (c) 1996-2001 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gsint.h"

/************************** Types and definitions ***************************/

struct PrinterCtxRec
{
  /* Output stream. */
  FILE *fp;

  /* If using temporary file, this is its name. */
  char tmpfile[512];

  /* Command to spool the temporary output.  This is NULL if we
     already spooled the output to `fp' and there is no
     post-processing to do. */
  Buffer *command;
};

typedef struct PrinterCtxRec PrinterCtxStruct;
typedef struct PrinterCtxRec *PrinterCtx;


/***************************** Global functions *****************************/

FILE *
printer_open(char *cmd, char *options, char *queue_param, char *printer_name,
	     void **context_return)
{
  PrinterCtx ctx;

  ctx = xcalloc(1, sizeof(*ctx));

  if (cmd && cmd[0])
    {
      if (tmpnam(ctx->tmpfile) == NULL)
	FATAL((stderr, _("could not create temporary spool file name: %s"),
	       strerror(errno)));

      /* Spool output to a temporary file and spool with with command
	 when the printer is closed. */

      ctx->command = buffer_alloc();

      buffer_append(ctx->command, cmd);
      buffer_append(ctx->command, " ");

      if (options)
	{
	  buffer_append(ctx->command, options);
	  buffer_append(ctx->command, " ");
	}

      if (printer_name)
	{
	  buffer_append(ctx->command, queue_param);
	  buffer_append(ctx->command, printer_name);
	  buffer_append(ctx->command, " ");
	}

      buffer_append(ctx->command, ctx->tmpfile);

      /* Open the temporary spool file. */
      ctx->fp = fopen(ctx->tmpfile, "wb");
      if (ctx->fp == NULL)
	FATAL((stderr, _("Could not open temporary spool file `%s': %s"),
	       ctx->tmpfile, strerror(errno)));
    }
  else
    {
      /* Just open file pointer to the printer. */
      ctx->fp = fopen(printer_name, "wb");
      if (ctx->fp == NULL)
	FATAL((stderr, _("Could not open printer `%s': %s"), printer_name,
	      strerror(errno)));
    }

  *context_return = ctx;

  return ctx->fp;
}


void
printer_close(void *context)
{
  PrinterCtx ctx = (PrinterCtx) context;

  /* Close the output stream. */
  fclose(ctx->fp);

  /* Do we need to do post-processing (read spooling). */
  if (ctx->command)
    {
      /* Yes. */
      if (system(buffer_ptr(ctx->command)) == -1)
	FATAL((stderr, _("Could not spool temporary output `%s': %s"),
	       ctx->tmpfile, strerror(errno)));

      /* We do not need the spool command anymore. */
      buffer_free(ctx->command);

      /* Unlink the temporary output file. */
      (void) remove(ctx->tmpfile);
    }

  /* Free context. */
  xfree(ctx);
}
