// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ServletHttpContextMBean.java,v 1.14.2.6 2003/06/04 04:47:54 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet.jmx;

import javax.management.MBeanException;

import org.mortbay.http.jmx.HttpContextMBean;


/* ------------------------------------------------------------ */
/** Web Application MBean.
 * Note that while Web Applications are HttpContexts, the MBean is
 * not derived from HttpContextMBean as they are managed differently.
 *
 * @version $Revision: 1.14.2.6 $
 * @author Greg Wilkins (gregw)
 */
public class ServletHttpContextMBean extends HttpContextMBean
{
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public ServletHttpContextMBean()
        throws MBeanException
    {}

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();

        defineOperation("addServlet",
                        new String[] {STRING,STRING,STRING},
                        IMPACT_ACTION);
    }
}
