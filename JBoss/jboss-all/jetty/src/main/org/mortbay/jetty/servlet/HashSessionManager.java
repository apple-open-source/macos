// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: HashSessionManager.java,v 1.16.2.5 2003/06/04 04:47:51 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.jetty.servlet;

import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import javax.servlet.http.HttpServletRequest;


/* ------------------------------------------------------------ */
/** An in-memory implementation of SessionManager.
 *
 * @version $Id: HashSessionManager.java,v 1.16.2.5 2003/06/04 04:47:51 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class HashSessionManager extends AbstractSessionManager
{
    /* ------------------------------------------------------------ */
    public HashSessionManager()
    {
        super();
    }
    
    /* ------------------------------------------------------------ */
    public HashSessionManager(Random random)
    {
        super(random);
    }

    /* ------------------------------------------------------------ */
    protected AbstractSessionManager.Session newSession(HttpServletRequest request)
    {
        return new Session(request);
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    protected class Session extends AbstractSessionManager.Session
    {
        /* ------------------------------------------------------------- */
        protected Session(HttpServletRequest request)
        {
            super(request);
        }
        
        /* ------------------------------------------------------------ */
        protected Map newAttributeMap()
        {
            return new HashMap(3);
        }
    }
    
}
