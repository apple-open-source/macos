/* $Xorg: reqstats.c,v 1.4 2001/02/09 02:04:05 xorgcvs Exp $ */

/*

Copyright 1996  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
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
/* $XFree86: xc/lib/lbxutil/lbx_zlib/reqstats.c,v 1.5 2001/08/27 19:01:07 dawes Exp $ */

#include "reqstats.h"

#ifdef LBXREQSTATS

#include <X11/Xfuncs.h>
#include <X11/Xproto.h>
#define _XLBX_SERVER_
#include "XLbx.h"
#include "lbx_zlib.h"
#include <signal.h>
#include <stdio.h>

#define LBX_CODE 136 /* XXX - this should not be hardcoded - on todo list */

extern unsigned long stream_in_compressed;
extern unsigned long stream_in_plain;
extern unsigned long stream_in_packet_header_bytes;
extern unsigned long stream_in_uncompressed_bytes;

unsigned long check_sum_in_compressed;
unsigned long check_sum_in_plain;
int unknown_extension_bytes = 0;

struct ReqStats CoreRequestStats[128];
struct ReqStats LbxRequestStats[LbxNumberReqs];

static void LbxPrintReqStats (int);

char *X_ReqNames[128] = {
    0,				    /*  0  */
    "CreateWindow",                 /*  1  */
    "ChangeWindowAttributes",       /*  2  */
    "GetWindowAttributes",          /*  3  */
    "DestroyWindow",                /*  4  */
    "DestroySubwindows",            /*  5  */
    "ChangeSaveSet",                /*  6  */
    "ReparentWindow",               /*  7  */
    "MapWindow",                    /*  8  */
    "MapSubwindows",                /*  9  */
    "UnmapWindow",                  /*  10  */
    "UnmapSubwindows",              /*  11  */
    "ConfigureWindow",              /*  12  */
    "CirculateWindow",              /*  13  */
    "GetGeometry",                  /*  14  */
    "QueryTree",                    /*  15  */
    "InternAtom",                   /*  16  */
    "GetAtomName",                  /*  17  */
    "ChangeProperty",               /*  18  */
    "DeleteProperty",               /*  19  */
    "GetProperty",                  /*  20  */
    "ListProperties",               /*  21  */
    "SetSelectionOwner",            /*  22  */
    "GetSelectionOwner",            /*  23  */
    "ConvertSelection",             /*  24  */
    "SendEvent",                    /*  25  */
    "GrabPointer",                  /*  26  */
    "UngrabPointer",                /*  27  */
    "GrabButton",                   /*  28  */
    "UngrabButton",                 /*  29  */
    "ChangeActivePointerGrab",      /*  30  */
    "GrabKeyboard",                 /*  31  */
    "UngrabKeyboard",               /*  32  */
    "GrabKey",                      /*  33  */
    "UngrabKey",                    /*  34  */
    "AllowEvents",                  /*  35  */
    "GrabServer",                   /*  36  */
    "UngrabServer",                 /*  37  */
    "QueryPointer",                 /*  38  */
    "GetMotionEvents",              /*  39  */
    "TranslateCoords",              /*  40  */
    "WarpPointer",                  /*  41  */
    "SetInputFocus",                /*  42  */
    "GetInputFocus",                /*  43  */
    "QueryKeymap",                  /*  44  */
    "OpenFont",                     /*  45  */
    "CloseFont",                    /*  46  */
    "QueryFont",                    /*  47  */
    "QueryTextExtents",             /*  48  */
    "ListFonts",                    /*  49  */
    "ListFontsWithInfo",    	    /*  50  */
    "SetFontPath",                  /*  51  */
    "GetFontPath",                  /*  52  */
    "CreatePixmap",                 /*  53  */
    "FreePixmap",                   /*  54  */
    "CreateGC",                     /*  55  */
    "ChangeGC",                     /*  56  */
    "CopyGC",                       /*  57  */
    "SetDashes",                    /*  58  */
    "SetClipRectangles",            /*  59  */
    "FreeGC",                       /*  60  */
    "ClearArea",                    /*  61  */
    "CopyArea",                     /*  62  */
    "CopyPlane",                    /*  63  */
    "PolyPoint",                    /*  64  */
    "PolyLine",                     /*  65  */
    "PolySegment",                  /*  66  */
    "PolyRectangle",                /*  67  */
    "PolyArc",                      /*  68  */
    "FillPoly",                     /*  69  */
    "PolyFillRectangle",            /*  70  */
    "PolyFillArc",                  /*  71  */
    "PutImage",                     /*  72  */
    "GetImage",                     /*  73  */
    "PolyText8",                    /*  74  */
    "PolyText16",                   /*  75  */
    "ImageText8",                   /*  76  */
    "ImageText16",                  /*  77  */
    "CreateColormap",               /*  78  */
    "FreeColormap",                 /*  79  */
    "CopyColormapAndFree",          /*  80  */
    "InstallColormap",              /*  81  */
    "UninstallColormap",            /*  82  */
    "ListInstalledColormaps",       /*  83  */
    "AllocColor",                   /*  84  */
    "AllocNamedColor",              /*  85  */
    "AllocColorCells",              /*  86  */
    "AllocColorPlanes",             /*  87  */
    "FreeColors",                   /*  88  */
    "StoreColors",                  /*  89  */
    "StoreNamedColor",              /*  90  */
    "QueryColors",                  /*  91  */
    "LookupColor",                  /*  92  */
    "CreateCursor",                 /*  93  */
    "CreateGlyphCursor",            /*  94  */
    "FreeCursor",                   /*  95  */
    "RecolorCursor",                /*  96  */
    "QueryBestSize",                /*  97  */
    "QueryExtension",               /*  98  */
    "ListExtensions",               /*  99  */
    "ChangeKeyboardMapping",        /*  100  */
    "GetKeyboardMapping",           /*  101  */
    "ChangeKeyboardControl",        /*  102  */
    "GetKeyboardControl",           /*  103  */
    "Bell",                         /*  104  */
    "ChangePointerControl",         /*  105  */
    "GetPointerControl",            /*  106  */
    "SetScreenSaver",               /*  107  */
    "GetScreenSaver",               /*  108  */
    "ChangeHosts",                  /*  109  */
    "ListHosts",                    /*  110  */
    "SetAccessControl",             /*  111  */
    "SetCloseDownMode",             /*  112  */
    "KillClient",                   /*  113  */
    "RotateProperties",	            /*  114  */
    "ForceScreenSaver",	            /*  115  */
    "SetPointerMapping",            /*  116  */
    "GetPointerMapping",            /*  117  */
    "SetModifierMapping",	    /*  118  */
    "GetModifierMapping",	    /*  119  */
    0,				    /*  120  */
    0,				    /*  121  */
    0,				    /*  122  */
    0,				    /*  123  */
    0,				    /*  124  */
    0,				    /*  125  */
    0,				    /*  126  */
    "NoOperation"                   /*  127  */
};


char *LBX_ReqNames[LbxNumberReqs] = {

    "LbxQueryVersion",		    /*  0  */
    "LbxStartProxy",		    /*  1  */
    "LbxStopProxy",		    /*  2  */
    "LbxSwitch",		    /*  3  */
    "LbxNewClient",		    /*  4  */
    "LbxCloseClient",		    /*  5  */
    "LbxModifySequence",	    /*  6  */
    "LbxAllowMotion",		    /*  7  */
    "LbxIncrementPixel",	    /*  8  */
    "LbxDelta",			    /*  9  */
    "LbxGetModifierMapping",	    /*  10  */
    "LbxQueryTag",		    /*  11  */
    "LbxInvalidateTag",		    /*  12  */
    "LbxPolyPoint",		    /*  13  */
    "LbxPolyLine",		    /*  14  */
    "LbxPolySegment",		    /*  15  */
    "LbxPolyRectangle",		    /*  16  */
    "LbxPolyArc",		    /*  17  */
    "LbxFillPoly",		    /*  18  */
    "LbxPolyFillRectangle",	    /*  19  */
    "LbxPolyFillArc",		    /*  20  */
    "LbxGetKeyboardMapping",	    /*  21  */
    "LbxQueryFont",		    /*  22  */
    "LbxChangeProperty",	    /*  23  */
    "LbxGetProperty",		    /*  24  */
    "LbxTagData",		    /*  25  */
    "LbxCopyArea",		    /*  26  */
    "LbxCopyPlane",		    /*  27  */
    "LbxPolyText8",		    /*  28  */
    "LbxPolyText16",		    /*  29  */
    "LbxImageText8",		    /*  30  */
    "LbxImageText16",		    /*  31  */
    "LbxQueryExtension",	    /*  32  */
    "LbxPutImage",		    /*  33  */
    "LbxGetImage",		    /*  34  */
    "LbxBeginLargeRequest",	    /*  35  */
    "LbxLargeRequestData",	    /*  36  */
    "LbxEndLargeRequest"	    /*  37  */
};



void
InitLbxReqStats (void)

{
    bzero (CoreRequestStats, 128 * sizeof (struct ReqStats));
    bzero (LbxRequestStats, LbxNumberReqs * sizeof (struct ReqStats));

    signal (SIGUSR1, LbxPrintReqStats);
}


static void
PrintStatsTable (struct ReqStats *table,
		 int count,
		 char **reqNames)

{
    int i;

    fprintf (stderr, "U  = uncompressed bytes\n");
    fprintf (stderr, "C  = compressed bytes\n");
    fprintf (stderr, "%%C = percent compression\n");
    fprintf (stderr, "%%T = percent of total bytes in stream\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "%-25s\tCount\tU\tC\t%%C\t\t%%T\n", "Request");
    fprintf (stderr, "-------------------------------------------------------------------------------\n");
 
    for (i = 0; i < count; i++)
    {
	float compRatio, percentTot;

	if (table[i].uncomp_bytes == 0)
	    compRatio = 0.0;
	else
	    compRatio = 100.0 * (1.0 - ((float) table[i].comp_bytes /
	        (float) table[i].uncomp_bytes));

	if (stream_in_compressed - stream_in_packet_header_bytes == 0)
	    percentTot = 0.0;
	else
	    percentTot = 100.0 * (float) table[i].comp_bytes /
	        (float) (stream_in_compressed -
			stream_in_packet_header_bytes);

	if (reqNames[i])
	    fprintf (stderr, "%-25s\t%d\t%d\t%d\t%.3f\t\t%.3f\n",
	        reqNames[i],
	        table[i].count,
	        table[i].uncomp_bytes,
	        table[i].comp_bytes,
	        compRatio,
                percentTot);

	check_sum_in_plain += table[i].uncomp_bytes;
	check_sum_in_compressed += table[i].comp_bytes;
    }
}

static void
PrintDeltaStats (struct ReqStats *table,
		 int count,
		 char **reqNames)

{
    int i;

    for (i = 0; i < count; i++)
    {
	if (table[i].delta_count && reqNames[i])
	    fprintf (stderr, "%-25s\t%d\t%d\t%d\n",
	        reqNames[i],
	        table[i].delta_count,
	        table[i].pre_delta_bytes,
	        table[i].post_delta_bytes);
    }
}


static void
LbxPrintReqStats (int dummy)

{
    unsigned long total;

    check_sum_in_plain = 0;
    check_sum_in_compressed = 0;

    fprintf (stderr, "\n\n");
    fprintf (stderr, "Core X requests\n\n");
    PrintStatsTable (CoreRequestStats, 128, X_ReqNames);
 
    fprintf (stderr, "\n\n");
    fprintf (stderr, "LBX requests\n\n");
    PrintStatsTable (LbxRequestStats, LbxNumberReqs, LBX_ReqNames);

    fprintf (stderr, "\n\n\n");

    fprintf (stderr, "The following requests were delta compressed:\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "Pre  = pre delta bytes\n");
    fprintf (stderr, "Post = post delta bytes\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "%-25s\tCount\tPre\tPost\n", "Request");
    fprintf (stderr, "-------------------------------------------------------------------------------\n");
    PrintDeltaStats (CoreRequestStats, 128, X_ReqNames);
    PrintDeltaStats (LbxRequestStats, LbxNumberReqs, LBX_ReqNames);

    fprintf (stderr, "\n\n\n");

    fprintf (stderr, "overall stream compression = %f %%\n", 
	(check_sum_in_plain == 0) ? 0.0 :
            (100.0 * (1.0 - ((float) check_sum_in_compressed /
	    (float) check_sum_in_plain))));

    fprintf (stderr, "\n\n\n");

    total = check_sum_in_plain + unknown_extension_bytes +
	stream_in_uncompressed_bytes;

    if (total == stream_in_plain)
	fprintf (stderr, "Internal checksum succeeded!!!\n");
    else
    {
	fprintf (stderr, "Internal checksum failed!!!\n");
	fprintf (stderr, "actual checksum = %d\n", stream_in_plain);
	fprintf (stderr, "computed checksum = %d\n", total);
    }

    fprintf (stderr, "\n\n");
}


void
do_decompress_with_stats (struct compress_private *priv)

{
    int incount = priv->cp_inputbufend - priv->cp_inputbuf;

    priv->stream.next_in = priv->cp_inputbuf;
    priv->stream.next_out = priv->cp_outputbuf;
    priv->stream.avail_out = priv->cp_outputbufend - priv->cp_outputbuf;

    while (incount > 0)
    {
	char *save_out = (char *) priv->stream.next_out;
	int outcount, outconsumed;

	priv->stream.avail_in = 1;
	priv->z_err = inflate (&(priv->stream), Z_NO_FLUSH);

	priv->need_flush_decompress = (priv->stream.avail_out == 0 );

	priv->req_compbytes_read++;

	outcount = (char *) priv->stream.next_out - save_out;

	priv->req_uncompbytes_read += outcount;

	while (outcount > 0)
	{
	    if (priv->req_length == -1 && priv->x_header_bytes_read < 4)
	    {
		int have = MIN (outcount, (4 - priv->x_header_bytes_read));
		memcpy (priv->x_header_buf + priv->x_header_bytes_read,
		    save_out, have);
		priv->x_header_bytes_read += have;

		if (priv->x_header_bytes_read < 4 && ((outcount - have) == 0))
		    break;
	    }

	    if (priv->req_length == -1 && priv->x_header_bytes_read == 4)
	    {
		xReq *req = (xReq *) priv->x_header_buf;

		priv->req_length = req->length << 2;
		priv->x_req_code = req->reqType;

		if (priv->x_req_code == LBX_CODE)
		    priv->lbx_req_code = req->data;
	    }

	    if (priv->req_length != -1)
	    {
		if (priv->req_uncompbytes_read < priv->req_length)
		    break;
		else
		{
		    if (priv->x_req_code == LBX_CODE)
		    {
			struct ReqStats *reqStat =
			    &LbxRequestStats[priv->lbx_req_code];

			reqStat->count++;
			reqStat->comp_bytes += priv->req_compbytes_read;
			reqStat->uncomp_bytes += priv->req_length;

#ifdef LBXREQLOG
			fprintf (stderr,
			    "LBX opcode = %d, real len = %d, comp len = %d\n",
			    priv->lbx_req_code, priv->req_length,
			    priv->req_compbytes_read);
#endif
		    }
		    else if (priv->x_req_code < 128)
		    {
			struct ReqStats *reqStat =
			    &CoreRequestStats[priv->x_req_code];

			reqStat->count++;
			reqStat->comp_bytes += priv->req_compbytes_read;
			reqStat->uncomp_bytes += priv->req_length;

#ifdef LBXREQLOG
			fprintf (stderr,
			    "X opcode = %d, real len = %d, comp len = %d\n",
			    priv->x_req_code, priv->req_length,
			    priv->req_compbytes_read);
#endif
		    }
		    else
		    {
#ifdef LBXREQLOG
			fprintf (stderr,
			    "Unknown opcode = %d, real len = %d, comp len = %d\n",
			    priv->x_req_code, priv->req_length,
			    priv->req_compbytes_read);
#endif
			unknown_extension_bytes += priv->req_length;
		    }

		    priv->req_uncompbytes_read -= priv->req_length;

		    if (priv->req_uncompbytes_read > 0)
		    {
			priv->req_compbytes_read = 1;
			save_out += (outcount - priv->req_uncompbytes_read);
			outcount = priv->req_uncompbytes_read;
		    }
		    else
		    {
			priv->req_compbytes_read = 0;
			outcount = 0;
		    }

		    priv->req_length = -1;
		    priv->x_header_bytes_read = 0;
		}
	    }
	}

	incount--;

	if (priv->stream.avail_out == 0)
	    break;
    }

    priv->cp_inputbuf = priv->stream.next_in;
    priv->cp_outputbuf = priv->stream.next_out;
}

#else

void
InitLbxReqStats()
{}

#endif /* LBXREQSTATS */
