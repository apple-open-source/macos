// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: NCSARequestLogMBean.java,v 1.14.2.6 2003/06/04 04:47:48 starksm Exp $
// ========================================================================

package org.mortbay.http.jmx;

import javax.management.MBeanException;

import org.mortbay.util.jmx.LifeCycleMBean;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.6 $
 * @author Greg Wilkins (gregw)
 */
public class NCSARequestLogMBean extends LifeCycleMBean
{
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException
     */
    public NCSARequestLogMBean()
        throws MBeanException
    {}    

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("filename");
        defineAttribute("datedFilename");
        defineAttribute("logDateFormat");
        defineAttribute("logTimeZone");
        defineAttribute("retainDays");
        defineAttribute("extended");
        defineAttribute("append");
    }
}
