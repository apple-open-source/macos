// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: SocketChannelListenerMBean.java,v 1.9.2.6 2003/06/04 04:47:48 starksm Exp $
// ========================================================================

package org.mortbay.http.jmx;

import javax.management.MBeanException;

import org.mortbay.util.jmx.ThreadPoolMBean;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.9.2.6 $
 * @author Greg Wilkins (gregw)
 */
public class SocketChannelListenerMBean extends ThreadPoolMBean
{
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public SocketChannelListenerMBean()
        throws MBeanException
    {
        super();
    }

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("host");
        defineAttribute("port");
        defineAttribute("maxReadTimeMs");
        defineAttribute("lingerTimeSecs");
        defineAttribute("lowOnResources");
        defineAttribute("outOfResources");
        defineAttribute("defaultScheme");
    }
}
