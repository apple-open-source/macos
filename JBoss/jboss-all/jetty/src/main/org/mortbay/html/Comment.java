// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Comment.java,v 1.15.2.3 2003/06/04 04:47:36 starksm Exp $
// ========================================================================

package org.mortbay.html;
import java.io.IOException;
import java.io.Writer;


/* ------------------------------------------------------------ */
/** HTML Comment.
 * @version $Id: Comment.java,v 1.15.2.3 2003/06/04 04:47:36 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class Comment extends Composite
{
    /* ----------------------------------------------------------------- */
    public void write(Writer out)
         throws IOException
    {
        out.write("<!--\n");
        super.write(out);
        out.write("\n-->");
    }
};
