/*
 * compiler/core/exports.c
 *
 *      ExportElmt list set up during  parse.
 *      (not kept in Module data struct)
 *
 *  SetExports runs through type, value & macro defs and sets the
 *  exported flag accordingly.
 *
 *  The exportsParsed boolean means whether the symbol "EXPORTS"
 *  was parsed - since if EXPORTS was parsed and the export list
 *  is empty, NOTHING is exported, otherwise if the "EXPORTS"
 *  symbol was not parsed (export list is empty) then EVERYTHING
 *  is exported.  If "EXPORTS" was parsed and the list is not
 *  empty, then mark each item is the list as exported and the
 *  rest (that are not in the list) as not exported.
 *
 * Mike Sample
 * 91/09/04
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/core/exports.c,v 1.1 2001/06/20 21:27:56 dmitch Exp $
 * $Log: exports.c,v $
 * Revision 1.1  2001/06/20 21:27:56  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:48  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 19:41:27  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:33:28  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:49:08  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "snacc-util.h"
#include "exports.h"

/*
 * called from main in snacc.c to set exported flags for
 * typeDefs and valueDefs in the given module
 */
void
SetExports PARAMS ((m, e, exportsParsed),
    Module *m _AND_
    ExportElmt *e _AND_
    int exportsParsed)
{
    TypeDef *td;
    ValueDef *vd;

    if (!exportsParsed)  /* export everything */
    {
        /*
         * set all typedefs', valuedefs' and macrodefs' exported flag
         */
        m->exportStatus = EXPORTS_ALL;
        FOR_EACH_LIST_ELMT (td, m->typeDefs)
        {
            td->exported = TRUE;
        }

        FOR_EACH_LIST_ELMT (vd, m->valueDefs)
        {
            vd->exported = TRUE;
        }
    }
    else  /* EXPORTS sym parsed */
    {
        /* init every exports flag to false */
        FOR_EACH_LIST_ELMT (td, m->typeDefs)
        {
            td->exported = FALSE;
        }
        FOR_EACH_LIST_ELMT (vd, m->valueDefs)
        {
            vd->exported = FALSE;
        }

        if (e == NULL) /* export nothing */
        {
            m->exportStatus = EXPORTS_NOTHING;
        }
        else /* just export types/values in export list */
        {
            m->exportStatus = EXPORTS_SOME;
            for (; e != NULL; e = e->next)
            {
                if ((td = LookupType (m->typeDefs,  e->name)) != NULL)
                    td->exported = TRUE;

                else if ((vd = LookupValue (m->valueDefs,  e->name)) != NULL)
                    vd->exported = TRUE;
                else
                {
                    PrintErrLoc (m->asn1SrcFileName, e->lineNo);
                    fprintf (stderr, "ERROR - exporting undefined type/value \"%s\"\n", e->name);
                }
            }
        }
    }
}  /* SetExports */
