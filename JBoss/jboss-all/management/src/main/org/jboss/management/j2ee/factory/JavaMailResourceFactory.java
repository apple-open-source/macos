/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import org.jboss.management.j2ee.JavaMailResource;

import javax.management.ObjectName;
import javax.management.MBeanServer;

/** A factory for JavaMailResource managed objects
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class JavaMailResourceFactory
   implements ManagedObjectFactory
{
   /** Creates a JavaMailResource given a ServiceController create notification
    * @param server
    * @param data The service ObjectName
    * @return the JNDIResource ObjectName
    */
   public ObjectName create(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      String resName = serviceName.getKeyProperty("name");
      if( resName == null )
         resName = "DefaultMail";
      ObjectName name = JavaMailResource.create(server, resName, serviceName);
      return name;
   }

   /** Creates a JavaMailResource given a ServiceController destroy notification
    * @param server
    * @param data The service ObjectName
    * @return the JNDIResource ObjectName
    */
   public void destroy(MBeanServer server, Object data)
   {
      ObjectName serviceName = (ObjectName) data;
      String resName = serviceName.getKeyProperty("name");
      if( resName == null )
         resName = "DefaultMail";
      JavaMailResource.destroy( server, resName);
   }
}
