// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AJP13ListenerMBean.java,v 1.3.2.6 2003/06/04 04:47:46 starksm Exp $
// ========================================================================

package org.mortbay.http.ajp.jmx;

import javax.management.MBeanException;

import org.mortbay.http.jmx.HttpListenerMBean;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.3.2.6 $
 * @author Greg Wilkins (gregw)
 */
public class AJP13ListenerMBean extends HttpListenerMBean
{
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public AJP13ListenerMBean()
        throws MBeanException
    {
        super();
    }

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("identifyListener");
        defineAttribute("remoteServers");
    }
}
