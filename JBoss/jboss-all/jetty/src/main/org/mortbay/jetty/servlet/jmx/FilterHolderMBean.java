// ========================================================================
// Copyright (c) 2002,2003 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: FilterHolderMBean.java,v 1.1.2.4 2003/06/04 04:47:54 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet.jmx;

import javax.management.MBeanException;

import org.mortbay.jetty.servlet.FilterHolder;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.1.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class FilterHolderMBean extends HolderMBean 
{
    /* ------------------------------------------------------------ */
    private FilterHolder _holder;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public FilterHolderMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("paths");
        defineAttribute("servlets");
        _holder=(FilterHolder)getManagedResource();
    }
    
}
