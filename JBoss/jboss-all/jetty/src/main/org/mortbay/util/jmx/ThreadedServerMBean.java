// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ThreadedServerMBean.java,v 1.14.2.5 2003/06/04 04:48:04 starksm Exp $
// ========================================================================

package org.mortbay.util.jmx;

import javax.management.MBeanException;
import org.mortbay.util.ThreadedServer;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.5 $
 * @author Greg Wilkins (gregw)
 */
public class ThreadedServerMBean extends ThreadPoolMBean
{
    /* ------------------------------------------------------------ */
    public ThreadedServerMBean()
        throws MBeanException
    {
        super();
    }
    
    /* ------------------------------------------------------------ */
    public ThreadedServerMBean(ThreadedServer object)
        throws MBeanException
    {
        super(object);
    }
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();

        defineAttribute("host");
        defineAttribute("port");
        defineAttribute("lingerTimeSecs");
    }    
}
