// ========================================================================
// Copyright (c) 2003 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: SessionManagerMBean.java,v 1.1.4.4 2003/06/04 04:47:54 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet.jmx;

import javax.management.MBeanException;
import org.mortbay.jetty.servlet.SessionManager;
import org.mortbay.util.jmx.LifeCycleMBean;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.1.4.4 $
 * @author Greg Wilkins (gregw)
 */
public class SessionManagerMBean extends LifeCycleMBean
{
    /* ------------------------------------------------------------ */
    public SessionManagerMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    public SessionManagerMBean(SessionManager object)
        throws MBeanException
    {
        super(object);
    }
}
