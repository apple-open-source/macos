// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: OutputObserver.java,v 1.1.2.4 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.io.IOException;
import java.io.OutputStream;

/* ------------------------------------------------------------ */
/** Observer output events.
 *
 * @see org.mortbay.http.HttpOutputStream
 * @version $Id: OutputObserver.java,v 1.1.2.4 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public interface OutputObserver
{
    public final static int
        __FIRST_WRITE=0,
        __RESET_BUFFER=1,
        __COMMITING=2,
        __CLOSING=4,
        __CLOSED=5;
    
    /* ------------------------------------------------------------ */
    /** Notify an output action.
     * @param out The OutputStream that caused the event
     * @param action The action taken
     * @param data Data associated with the event.
     */
    void outputNotify(OutputStream out, int action, Object data)
        throws IOException;
}
