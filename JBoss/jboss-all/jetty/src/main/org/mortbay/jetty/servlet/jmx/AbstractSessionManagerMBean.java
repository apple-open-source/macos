// ========================================================================
// Copyright (c) 2003 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AbstractSessionManagerMBean.java,v 1.1.4.7 2003/07/11 00:55:03 jules_gosnell Exp $
// ========================================================================

package org.mortbay.jetty.servlet.jmx;

import javax.management.MBeanException;

import org.mortbay.jetty.servlet.SessionManager;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.1.4.7 $
 * @author Greg Wilkins (gregw)
 */
public class AbstractSessionManagerMBean extends SessionManagerMBean
{
    /* ------------------------------------------------------------ */
    public AbstractSessionManagerMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    public AbstractSessionManagerMBean(SessionManager object)
        throws MBeanException
    {
        super(object);
    }
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("maxInactiveInterval"); 
        defineAttribute("scavengePeriod"); 
        defineAttribute("useRequestedId"); 
        defineAttribute("workerName");  
        defineAttribute("sessions"); 
        defineAttribute ("minSessions");
        defineAttribute ("maxSessions");
        defineOperation ("resetStats",IMPACT_ACTION);
    }

}
