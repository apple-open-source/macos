/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import org.jboss.management.j2ee.JNDIResource;
import org.jboss.management.j2ee.JTAResource;

import javax.management.ObjectName;
import javax.management.MBeanServer;

/** The JSR77.3.30 JTAResource factory
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class JTAResourceFactory
   implements ManagedObjectFactory
{
   /** Creates a JTAResource given an MBeanServerNotification
    * @param server
    * @param data A MBeanServerNotification
    * @return the JNDIResource ObjectName
    */
   public ObjectName create(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      String name = serviceName.getKeyProperty("service");
      ObjectName jsr77Name = JTAResource.create(server, name, serviceName);
      return jsr77Name;
   }

   /** Destroys a JTAResource given an MBeanServerNotification
    * @param server
    * @param data A MBeanServerNotification
    * @return the JNDIResource ObjectName
    */
   public void destroy(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      String name = serviceName.getKeyProperty("service");
      JTAResource.destroy(server, name);
   }
}
