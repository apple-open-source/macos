// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: SocketListenerMBean.java,v 1.14.2.6 2003/06/04 04:47:48 starksm Exp $
// ========================================================================

package org.mortbay.http.jmx;

import javax.management.MBeanException;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.6 $
 * @author Greg Wilkins (gregw)
 */
public class SocketListenerMBean extends HttpListenerMBean
{
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public SocketListenerMBean()
        throws MBeanException
    {
        super();
    }

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("lowResourcePersistTimeMs");
        defineAttribute("identifyListener");
    }
}
