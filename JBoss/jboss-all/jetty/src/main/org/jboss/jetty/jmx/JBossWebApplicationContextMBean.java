// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: JBossWebApplicationContextMBean.java,v 1.1.2.3 2003/02/15 23:23:37 jules_gosnell Exp $
// ========================================================================

package org.jboss.jetty.jmx;

import java.util.Map;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.ObjectName;
import javax.management.RuntimeOperationsException;
import javax.management.modelmbean.InvalidTargetObjectTypeException;
import org.jboss.jetty.JBossWebApplicationContext;
import org.mortbay.jetty.servlet.jmx.WebApplicationContextMBean;

/* ------------------------------------------------------------ */
/** JBoss Web Application MBean.
 *
 * @version $Revision: 1.1.2.3 $
 * @author Jules Gosnell (jules)
 */
public class
  JBossWebApplicationContextMBean
  extends WebApplicationContextMBean
{
  /* ------------------------------------------------------------ */
  /** Constructor.
   * @exception MBeanException
   * @exception InstanceNotFoundException
   */
  public
    JBossWebApplicationContextMBean()
    throws MBeanException
    {
    }

  /* ------------------------------------------------------------ */
  protected void
    defineManagedResource()
    {
      super.defineManagedResource();

      //         defineAttribute("displayName",false);
      //         defineAttribute("defaultsDescriptor",true);
      //         defineAttribute("deploymentDescriptor",false);
      //         defineAttribute("WAR",true);
      //         defineAttribute("extractWAR",true);
    }

   public void setManagedResource(Object proxyObject, String type)
     throws MBeanException,
     RuntimeOperationsException,
     InstanceNotFoundException,
     InvalidTargetObjectTypeException
     {
       super.setManagedResource(proxyObject, type);
       JBossWebApplicationContext jbwac=(JBossWebApplicationContext)proxyObject;
       jbwac.setMBeanPeer(this);
     }

   public ObjectName[]
     getComponentMBeans(Object[] components, Map map)
     {
       return super.getComponentMBeans(components, map);
     }
}
