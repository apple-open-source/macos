/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import java.util.HashMap;
import java.util.Iterator;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import org.jboss.deployment.DeploymentInfo;
import org.jboss.logging.Logger;
import org.jboss.management.j2ee.EJBModule;
import org.jboss.management.j2ee.EJB;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.SessionMetaData;
import org.jboss.ejb.EjbModule;

/** A factory for mapping EJBDeployer deployments to EJBModule
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.8 $
 */
public class EJBModuleFactory
   implements ManagedObjectFactory
{
   private static Logger log = Logger.getLogger(EJBModuleFactory.class);
   private static HashMap moduleServiceToMgmtMap = new HashMap();
   private HashMap deploymentToModuleNameMap = new HashMap();
   private HashMap containerToModuleNameMap = new HashMap();

   static ObjectName getEJBModuleName(ObjectName ejbModuleService)
   {
      ObjectName jsr77Name = (ObjectName) moduleServiceToMgmtMap.get(ejbModuleService);
      return jsr77Name;
   }

   /** Create JSR-77 EJBModule
    *
    * @param server the MBeanServer context
    * @param data arbitrary data associated with the creation context
    */
   public ObjectName create(MBeanServer server, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return null;

      DeploymentInfo di = (DeploymentInfo) data;
      String ejbJarName = di.shortName;
      ObjectName ejbModuleService = di.deployedObject;
      ObjectName jsr77Name = EJBModule.create(server,
            ( di.parent == null ? null : di.parent.shortName ),
            ejbJarName,
            di.localUrl,
            ejbModuleService);
      moduleServiceToMgmtMap.put(ejbModuleService, jsr77Name);
      deploymentToModuleNameMap.put(di, jsr77Name);
      log.debug("Created module: " + jsr77Name);
      Iterator ejbs = di.mbeans.iterator();
      while( ejbs.hasNext() )
      {
         ObjectName containerName = (ObjectName) ejbs.next();
         createEJB(server, containerName);
      }
      return jsr77Name;
   }

   /** Destroy JSR-77 EJBModule
    *
    * @param server the MBeanServer context
    * @param data arbitrary data associated with the creation context
    */
   public void destroy(MBeanServer server, Object data)
   {
      if( (data instanceof DeploymentInfo) == false )
         return;

      DeploymentInfo di = (DeploymentInfo) data;
      ObjectName jsr77Name = (ObjectName) deploymentToModuleNameMap.get(di);

      log.debug("Destroy module: " + jsr77Name);
      Iterator ejbs = di.mbeans.iterator();
      while( ejbs.hasNext() )
      {
         ObjectName containerName = (ObjectName) ejbs.next();
         destroyEJB(server, containerName);
      }

      if( jsr77Name != null )
      {
         EJBModule.destroy(server, jsr77Name);
      }
   }

   public ObjectName createEJB(MBeanServer server, ObjectName containerName)
   {
      ObjectName jsr77Name = null;
      try
      {
         BeanMetaData metaData = (BeanMetaData) server.getAttribute(containerName, "BeanMetaData");
         EjbModule ejbModule = (EjbModule) server.getAttribute(containerName, "EjbModule");
         ObjectName ejbModName = EJBModuleFactory.getEJBModuleName(ejbModule.getServiceName());
         int type =  EJB.STATELESS_SESSION_BEAN;
         if( metaData.isSession() )
         {
            SessionMetaData smetaData = (SessionMetaData) metaData;
            if( smetaData.isStateful() )
               type = EJB.STATEFUL_SESSION_BEAN;
         }
         else if ( metaData.isMessageDriven() )
            type = EJB.MESSAGE_DRIVEN_BEAN;
         else
            type = EJB.ENTITY_BEAN;

         jsr77Name = EJB.create(server, ejbModName, containerName,
            type, metaData.getContainerObjectNameJndiName());
         containerToModuleNameMap.put(containerName, jsr77Name);
         log.debug("Create container: "+containerName+", module: " + jsr77Name);
      }
      catch(Exception e)
      {
         log.debug("", e);
      }

      return jsr77Name;
   }

   /** Destory JSR-77 J2EEApplication
    *
    * @param server the MBeanServer context
    * @param data arbitrary data associated with the creation context
    */
   public void destroyEJB(MBeanServer server, ObjectName containerName)
   {
      ObjectName jsr77Name = (ObjectName) containerToModuleNameMap.get(containerName);

      log.debug("Destroy container: "+containerName+", module: " + jsr77Name);
      if( jsr77Name != null )
      {
         EJB.destroy(server, jsr77Name);
      }
   }

}
