/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import java.util.HashMap;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.deployment.DeploymentInfo;
import org.jboss.logging.Logger;
import org.jboss.management.j2ee.ResourceAdapterModule;
import org.jboss.management.j2ee.ResourceAdapter;
import org.jboss.resource.RARMetaData;

/** A factory for mapping RARDeployer deployments to ResourceAdaptorModules
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class RARModuleFactory
   implements ManagedObjectFactory
{
   private static Logger log = Logger.getLogger(RARModuleFactory.class);
   private static HashMap moduleServiceToMgmtMap = new HashMap();
   private HashMap deploymentToModuleNameMap = new HashMap();

   static ObjectName getResourceAdapterName(ObjectName rarService)
   {
      ObjectName jsr77Name = (ObjectName) moduleServiceToMgmtMap.get(rarService);
      return jsr77Name;
   }

   /** Create JSR-77 EJBModule
    *
    * @param server the MBeanServer context
    * @param data arbitrary data associated with the creation context
    */
   public ObjectName create(MBeanServer mbeanServer, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return null;

      DeploymentInfo di = (DeploymentInfo) data;
      RARMetaData metaData = (RARMetaData) di.metaData;

      // Create the ResourceAdapterModule
      String rarName = di.shortName;
      ObjectName rarService = di.deployedObject;
      ObjectName jsr77ModuleName = ResourceAdapterModule.create(mbeanServer,
            ( di.parent == null ? null : di.parent.shortName ),
            rarName,
            di.localUrl);
      deploymentToModuleNameMap.put(di, jsr77ModuleName);
      log.debug("Created module: " + jsr77ModuleName);

      // Create the ResourceAdapter
      ObjectName jsr77RAName = ResourceAdapter.create(mbeanServer,
         metaData.getDisplayName(), jsr77ModuleName, rarService);
      // Register a mapping from the RARDeployment service to the ResourceAdapter
      moduleServiceToMgmtMap.put(rarService, jsr77RAName);

      return jsr77ModuleName;
   }

   /** Destroy JSR-77 EJBModule
    *
    * @param server the MBeanServer context
    * @param data arbitrary data associated with the creation context
    */
   public void destroy(MBeanServer mbeanServer, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return;

      DeploymentInfo di = (DeploymentInfo) data;
      RARMetaData metaData = (RARMetaData) di.metaData;
      ObjectName jsr77Name = (ObjectName) deploymentToModuleNameMap.get(di);

      ResourceAdapter.destroy(mbeanServer, metaData.getDisplayName());
      log.debug("Destroy module: " + jsr77Name);
      if( jsr77Name != null )
      {
         ResourceAdapterModule.destroy(mbeanServer, jsr77Name);
      }
   }

}
