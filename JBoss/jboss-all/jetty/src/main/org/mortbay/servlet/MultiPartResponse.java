// ========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: MultiPartResponse.java,v 1.15.2.4 2003/06/04 04:47:56 starksm Exp $
// ------------------------------------------------------------------------

package org.mortbay.servlet;

import java.io.IOException;
import javax.servlet.http.HttpServletResponse;



/* ================================================================ */
/** Handle a multipart MIME response.
 *
 *
 * @version $Id: MultiPartResponse.java,v 1.15.2.4 2003/06/04 04:47:56 starksm Exp $
 * @author Greg Wilkins
 * @author Jim Crossley
*/
public class MultiPartResponse extends org.mortbay.http.MultiPartResponse
{
    /* ------------------------------------------------------------ */
    /** MultiPartResponse constructor.
     * @param response The ServletResponse to which this multipart
     *                 response will be sent.
     */
    public MultiPartResponse(HttpServletResponse response)
         throws IOException
    {
        super(response.getOutputStream());
        response.setContentType("multipart/mixed;boundary="+getBoundary());
    }
    
};




