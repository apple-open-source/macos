// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: LogSinkMBean.java,v 1.14.2.4 2003/06/04 04:48:04 starksm Exp $
// ========================================================================

package org.mortbay.util.jmx;

import javax.management.MBeanException;
import org.mortbay.util.LogSink;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class LogSinkMBean extends LifeCycleMBean
{

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public LogSinkMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public LogSinkMBean(LogSink logSink)
        throws MBeanException
    {
        super(logSink);
    }


    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineOperation("log",
                        new String[]{STRING},
                        IMPACT_ACTION);
    }
}
