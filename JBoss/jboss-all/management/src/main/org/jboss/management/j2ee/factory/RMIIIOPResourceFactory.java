/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import org.jboss.management.j2ee.RMI_IIOPResource;

import javax.management.ObjectName;
import javax.management.MBeanServer;

/** The factory for the JSR77.3.31 RMI_IIOPResource model objects
 *  
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.1 $
 */
public class RMIIIOPResourceFactory
     implements ManagedObjectFactory
{
   /** Creates a RMI_IIOPResource given an MBeanServerNotification
    * @param server
    * @param data A MBeanServerNotification
    * @return the RMI_IIOPResource ObjectName
    */
   public ObjectName create(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      String name = serviceName.getKeyProperty("service");
      ObjectName jsr77Name = RMI_IIOPResource.create(server, name, serviceName);
      return jsr77Name;
   }

   /** Destroys a RMI_IIOPResource given an MBeanServerNotification
    * @param server
    * @param data A MBeanServerNotification
    */
   public void destroy(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      String name = serviceName.getKeyProperty("service");
      RMI_IIOPResource.destroy(server, name);
   }
}
