// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ThreadPoolMBean.java,v 1.14.2.5 2003/06/04 04:48:04 starksm Exp $
// ========================================================================

package org.mortbay.util.jmx;

import javax.management.MBeanException;
import org.mortbay.util.ThreadPool;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.5 $
 * @author Greg Wilkins (gregw)
 */
public class ThreadPoolMBean extends LifeCycleMBean
{
    /* ------------------------------------------------------------ */
    public ThreadPoolMBean()
        throws MBeanException
    {
        super();
    }
    
    /* ------------------------------------------------------------ */
    public ThreadPoolMBean(ThreadPool object)
        throws MBeanException
    {
        super(object);
    }
    
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("name");
        defineAttribute("poolName");
        defineAttribute("threads");
        defineAttribute("idleThreads");
        defineAttribute("minThreads");
        defineAttribute("maxThreads");
        defineAttribute("maxIdleTimeMs");
    }    
}
