// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: LifeCycleMBean.java,v 1.14.2.4 2003/06/04 04:48:03 starksm Exp $
// ========================================================================

package org.mortbay.util.jmx;

import javax.management.MBeanException;
import javax.management.MBeanOperationInfo;
import org.mortbay.util.LifeCycle;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.14.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class LifeCycleMBean extends ModelMBeanImpl
{
    /* ------------------------------------------------------------ */
    public LifeCycleMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    public LifeCycleMBean(LifeCycle object)
        throws MBeanException
    {
        super(object);
    }
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        defineAttribute("started");
        defineOperation("start",MBeanOperationInfo.ACTION);
        defineOperation("stop",MBeanOperationInfo.ACTION);
    }    
}



