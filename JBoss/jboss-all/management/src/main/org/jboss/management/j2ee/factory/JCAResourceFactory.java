/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import java.util.Iterator;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import org.jboss.logging.Logger;
import org.jboss.management.j2ee.JCAResource;
import org.jboss.deployment.DeploymentInfo;

/** A factory for mapping DataSourceDeployer deployments to JCAResource
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class JCAResourceFactory
   implements ManagedObjectFactory
{
   private static Logger log = Logger.getLogger(JCAResourceFactory.class);

   /** Creates a JCAResource
    * @param server
    * @param data A MBeanServerNotification
    * @return the JCAResource ObjectName
    */
   public ObjectName create(MBeanServer mbeanServer, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return null;

      DeploymentInfo di = (DeploymentInfo) data;
      ObjectName jsr77Name = null;

      /* Get the RARDeployment service name by looking for the mbean in the
      deployment with a name matching service=xxxDS. This relies on the naming
      pattern created by the JCA CM deployer.
      */
      ObjectName rarDeployService = null;
      ObjectName cmService = null;
      ObjectName poolService = null;
      Iterator iter = di.mbeans.iterator();
      while( iter.hasNext() )
      {
         ObjectName oname = (ObjectName) iter.next();
         String name = oname.getKeyProperty("service");
         if( name.equals("ManagedConnectionFactory") || name.endsWith("DS") )
            rarDeployService = oname;
         else if( name.endsWith("CM") )
            cmService = oname;
         else if( name.endsWith("Pool") )
            poolService = oname;
      }
      if( rarDeployService == null || cmService == null )
      {
         log.debug("Failed to find a service=xxxDS match");
         return null;
      }

      try
      {
         /* Now to tie this CM back to its rar query the rarDeployService for
         the org.jboss.resource.RARDeployment service created by the RARDeployer.
          */
         ObjectName rarService = (ObjectName) mbeanServer.getAttribute(rarDeployService,
               "OldRarDeployment");
         // Get the ResourceAdapter JSR77 name
         ObjectName jsr77RAName = RARModuleFactory.getResourceAdapterName(rarService);
         // Now build the JCAResource
         String resName = rarDeployService.getKeyProperty("name");
         jsr77Name = JCAResource.create(mbeanServer, resName, jsr77RAName,
               cmService, rarDeployService, poolService);
      }
      catch(Exception e)
      {
         log.debug("", e);
      }

      return jsr77Name;
   }

   /** Destroys the JCAResource
    * @param server
    * @param data A MBeanServerNotification
    * @return the JCAResource ObjectName
    */
   public void destroy(MBeanServer server, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return;

      DeploymentInfo di = (DeploymentInfo) data;

   }
}
