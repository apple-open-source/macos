/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import org.jboss.management.j2ee.JNDIResource;

import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MBeanServerNotification;

/** A factory for JNDIResource managed objects
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class JNDIResourceFactory
   implements ManagedObjectFactory
{
   /** Creates a JNDIResource given an MBeanServerNotification
    * @param server
    * @param data A MBeanServerNotification
    * @return the JNDIResource ObjectName
    */
   public ObjectName create(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      ObjectName name = JNDIResource.create(server, "LocalJNDI", serviceName);
      return name;
   }

   /** Creates a JNDIResource given an MBeanServerNotification
    * @param server
    * @param data A MBeanServerNotification
    * @return the JNDIResource ObjectName
    */
   public void destroy(MBeanServer server, Object data)
   {
      JNDIResource.destroy( server, "LocalJNDI" );
   }
}
