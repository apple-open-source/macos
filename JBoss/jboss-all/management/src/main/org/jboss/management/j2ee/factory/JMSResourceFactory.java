/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import org.jboss.management.j2ee.JNDIResource;
import org.jboss.management.j2ee.JMSResource;

import javax.management.ObjectName;
import javax.management.MBeanServer;

/** A factory for JMSResource managed objects
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class JMSResourceFactory
   implements ManagedObjectFactory
{
   /** Creates a "LocalJMS" JMSResource associated with the ServiceController
    * create notification
    * @param server
    * @param data A MBeanServerNotification
    * @return the JNDIResource ObjectName
    */
   public ObjectName create(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      ObjectName name = JMSResource.create(server, "LocalJMS", serviceName);
      return name;
   }

   /** Destroys the "LocalJMS" JMSResource
    * @param server
    * @param data A MBeanServerNotification
    * @return the JNDIResource ObjectName
    */
   public void destroy(MBeanServer server, Object data)
   {
      JMSResource.destroy(server, "LocalJMS");
   }
}
