/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import java.util.HashMap;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import org.jboss.deployment.DeploymentInfo;
import org.jboss.logging.Logger;
import org.jboss.management.j2ee.J2EEApplication;

/** A factory for mapping EARDeployer deployments to J2EEApplications
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.4 $
 */
public class EARModuleFactory
   implements ManagedObjectFactory
{
   private static Logger log = Logger.getLogger(EARModuleFactory.class);

   private HashMap deploymentToModuleNameMap = new HashMap();

   /** Create JSR-77 J2EEApplication
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
      ObjectName jsr77Name = J2EEApplication.create(server, moduleName, di.localUrl);
      deploymentToModuleNameMap.put(di, jsr77Name);
      if( jsr77Name != null )
      {
         log.debug( "Created J2EEApplication: " + jsr77Name );
      }

      return jsr77Name;
   }

   /** Destroy JSR-77 J2EEApplication
    *
    * @param server the MBeanServer context
    * @param data arbitrary data associated with the creation context
    */
   public void destroy(MBeanServer server, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return;

      DeploymentInfo di = (DeploymentInfo) data;
      ObjectName jsr77Name = (ObjectName) deploymentToModuleNameMap.remove(di);
      // Remove JSR-77 EAR-Module
      if( jsr77Name != null )
      {
         J2EEApplication.destroy(server, jsr77Name);
         log.debug("Removed J2EEApplication: " + jsr77Name);
      }
   }
}
