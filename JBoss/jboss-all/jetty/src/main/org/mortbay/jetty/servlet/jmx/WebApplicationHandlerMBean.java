// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: WebApplicationHandlerMBean.java,v 1.1.4.6 2003/06/04 04:47:54 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet.jmx;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.management.MBeanException;
import javax.management.ObjectName;

import org.mortbay.jetty.servlet.WebApplicationHandler;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.1.4.6 $
 * @author Greg Wilkins (gregw)
 */
public class WebApplicationHandlerMBean extends ServletHandlerMBean
{
    /* ------------------------------------------------------------ */
    private WebApplicationHandler _webappHandler;
    private Map _filters = new HashMap();
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public WebApplicationHandlerMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("acceptRanges"); 
        defineAttribute("filters",READ_ONLY,ON_MBEAN);
        _webappHandler=(WebApplicationHandler)getManagedResource();
    }

    /* ------------------------------------------------------------ */
    public ObjectName[] getFilters()
    {
        List l=_webappHandler.getFilters();
        return getComponentMBeans(l.toArray(),_filters);  
    }
}
