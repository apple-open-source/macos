/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import java.util.HashMap;
import java.util.Iterator;
import javax.management.Notification;
import javax.management.ObjectName;

import org.jboss.deployment.DeploymentInfo;
import org.jboss.logging.Logger;
import org.jboss.mx.util.ObjectNameMatch;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.12 $
 */
public class DefaultManagedObjectFactoryMap
   implements ManagedObjectFactoryMap
{
   private static Logger log = Logger.getLogger(DefaultManagedObjectFactoryMap.class);
   private HashMap factoryMap = new HashMap();
   private HashMap patternFactoryMap = new HashMap();

   /** Obtain the ManagedObjectFactory approriate for the event notification
    * from the core JBoss layer. This method looks to the userData of the
    * event to figure out how to obtain the JMX ObjectName of the core JBoss
    * component sending the event. This is the current mechanism by which we
    * map from the JBoss core layer into JSR-77 managed objects.
    *
    * @param createEvent
    * @return The ManagedObjectFactory if found, null otherwise
    */
   public ManagedObjectFactory getFactory(Notification createEvent)
   {
      ManagedObjectFactory factory = null;

      Object data = createEvent.getUserData();
      ObjectName senderName = null;
      if( data instanceof ObjectName )
      {
         senderName = (ObjectName) data;
      }
      else if( data instanceof DeploymentInfo )
      {
         DeploymentInfo di = (DeploymentInfo) data;
         senderName = di.deployer.getServiceName();
      }
      factory = (ManagedObjectFactory) factoryMap.get(senderName);
      if( factory == null )
      {
         // Check the pattern to factory mappings
         Iterator iter = patternFactoryMap.keySet().iterator();
         while( iter.hasNext() )
         {
            ObjectName pattern = (ObjectName) iter.next();
            if( ObjectNameMatch.match(pattern, senderName) )
               factory = (ManagedObjectFactory) patternFactoryMap.get(pattern);
         }
         if( factory == null )
            log.debug("Failed to find factory for event: "+createEvent);
      }
      return factory;
   }

   public void setSARDeployer(ObjectName name)
   {
      factoryMap.put(name, new ServiceModuleFactory());
   }

   public void setEARDeployer(ObjectName name)
   {
      factoryMap.put(name, new EARModuleFactory());
   }

   public void setEJBDeployer(ObjectName name)
   {
      factoryMap.put(name, new EJBModuleFactory());
   }
   public void setRARDeployer(ObjectName name)
   {
      factoryMap.put(name, new RARModuleFactory());
   }
   public void setCMDeployer(ObjectName name)
   {
      factoryMap.put(name, new JCAResourceFactory());
   }

   public void setWARDeployer(ObjectName name)
   {
      factoryMap.put(name, new WebModuleFactory());
   }

   public void setJavaMailResource(ObjectName name)
   {
      factoryMap.put(name, new JavaMailResourceFactory());            
   }
   public void setJMSResource(ObjectName name)
   {
      factoryMap.put(name, new JMSResourceFactory());      
   }
   public void setJNDIResource(ObjectName name)
   {
      factoryMap.put(name, new JNDIResourceFactory());
   }
   public void setJTAResource(ObjectName name)
   {
      factoryMap.put(name, new JTAResourceFactory());
   }
   public void setRMI_IIOPResource(ObjectName name)
   {
      factoryMap.put(name,new RMIIIOPResourceFactory());
   }
}
