/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import java.util.List;
import java.util.ListIterator;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import org.jboss.deployment.DeploymentInfo;
import org.jboss.logging.Logger;
import org.jboss.management.j2ee.MBean;
import org.jboss.management.j2ee.ServiceModule;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.5 $
 */
public class ServiceModuleFactory
   implements ManagedObjectFactory
{
   private static Logger log = Logger.getLogger(ServiceModuleFactory.class);

   /** Create JSR-77 SAR-Module
    *
    * @param server the MBeanServer context
    * @param data arbitrary data associated with the creation context
    */
   public ObjectName create(MBeanServer server, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return null;

      DeploymentInfo di = (DeploymentInfo) data;
      String moduleName = di.shortName;
      ObjectName sarName = ServiceModule.create(server, moduleName, di.localUrl);
      if( sarName != null )
      {
         log.debug( "Created ServiceModule: " + sarName );
      }

      try
      {
         List mbeans = di.mbeans;
         for( int i = 0; i < mbeans.size(); i++ )
         {
            ObjectName mbeanName = (ObjectName) mbeans.get(i);
            // Create JSR-77 MBean
            MBean.create(server, sarName.toString(), mbeanName);
            log.debug("Create MBean, name: " + mbeanName + ", SAR Module: " + sarName);
         }
      }
      catch(Throwable e)
      {
         log.debug("Failed to create MBean, sarName:"+sarName, e);
      }

      return sarName;
   }

   public void destroy(MBeanServer server, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return;

      DeploymentInfo di = (DeploymentInfo) data;
      List services = di.mbeans;
      int lastService = services.size();

      for (ListIterator i = services.listIterator(lastService); i.hasPrevious();)
      {
         ObjectName name = (ObjectName) i.previous();
         try
         {
            // Destroy JSR-77 MBean
            MBean.destroy(server, name.toString());
            log.debug("Destroy MBean, name: " + name);
         }
         catch (Throwable e)
         {
            log.debug("Failed to remove remove JSR-77 MBean", e);
         } // end of try-catch
      }

      // Remove JSR-77 SAR-Module
      String moduleName = di.shortName;
      try
      {
         ServiceModule.destroy(server, moduleName);
         log.debug("Removed JSR-77 SAR: " + moduleName);
      }
      catch(Throwable e)
      {
         log.debug("Failed to remove JSR-77 SAR: " + moduleName);
      }
   }
}
