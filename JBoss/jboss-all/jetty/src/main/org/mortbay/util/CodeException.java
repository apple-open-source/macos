// ========================================================================
// Copyright (c) 1997 MortBay Consulting, Sydney
// $Id: CodeException.java,v 1.15.2.3 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;

/* ------------------------------------------------------------ */
/** Code Exception.
 * 
 * Thrown by Code.assert or Code.fail
 * @see Code
 * @version  $Id: CodeException.java,v 1.15.2.3 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins
 */
public class CodeException extends RuntimeException
{
    /* ------------------------------------------------------------ */
    /** Default constructor. 
     */
    public CodeException()
    {
        super();
    }

    public CodeException(String msg)
    {
        super(msg);
    }    
}

