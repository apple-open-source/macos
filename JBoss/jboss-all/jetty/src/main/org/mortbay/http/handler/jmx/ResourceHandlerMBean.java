// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ResourceHandlerMBean.java,v 1.1.4.3 2003/06/04 04:47:48 starksm Exp $
// ========================================================================

package org.mortbay.http.handler.jmx;

import javax.management.MBeanException;
import org.mortbay.http.jmx.HttpHandlerMBean;

/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.1.4.3 $
 * @author Greg Wilkins (gregw)
 */
public class ResourceHandlerMBean extends HttpHandlerMBean  
{
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public ResourceHandlerMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("allowedMethods"); 
        defineAttribute("dirAllowed"); 
        defineAttribute("acceptRanges"); 
        defineAttribute("redirectWelcome"); 
        defineAttribute("minGzipLength"); 
    }    
}
