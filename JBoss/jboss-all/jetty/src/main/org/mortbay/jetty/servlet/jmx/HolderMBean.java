// ========================================================================
// Copyright (c) 2002,2003 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: HolderMBean.java,v 1.1.2.4 2003/06/04 04:47:54 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet.jmx;

import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.mortbay.jetty.servlet.Holder;
import org.mortbay.util.Code;
import org.mortbay.util.jmx.LifeCycleMBean;


/* ------------------------------------------------------------ */
/** 
 *
 * @version $Revision: 1.1.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class HolderMBean extends LifeCycleMBean  
{
    /* ------------------------------------------------------------ */
    private Holder _holder;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     */
    public HolderMBean()
        throws MBeanException
    {}
    
    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        
        defineAttribute("name");
        defineAttribute("displayName");
        defineAttribute("className");
        defineAttribute("initParameters",READ_ONLY,ON_MBEAN);
        
        _holder=(Holder)getManagedResource();
    }
    
    /* ---------------------------------------------------------------- */
    public String getInitParameters()
    {
        return ""+_holder.getInitParameters();
    }
    
    /* ------------------------------------------------------------ */
    public synchronized ObjectName uniqueObjectName(MBeanServer server,
                                                    String objectName)
    {
        try
        {
            String name=_holder.getDisplayName();
            if (name==null || name.length()==0)
                name=_holder.getClassName();
            return new ObjectName(objectName+",name="+name);
        }
        catch(Exception e)
        {
            Code.warning(e);
            return super.uniqueObjectName(server,objectName);
        }
    }
}
