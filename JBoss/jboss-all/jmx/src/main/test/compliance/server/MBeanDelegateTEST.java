/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server;

import junit.framework.TestCase;

import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.NotificationFilter;

import test.compliance.server.support.Test;

/**
 * Tests for the MBean server delegate.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 *
 * @version $Revision: 1.2 $
 */
public class MBeanDelegateTEST extends TestCase
{

   public MBeanDelegateTEST(String s)
   {
      super(s);
   }
   
   class MyNotificationListener implements NotificationListener {

      int notificationCount = 0;
      
      public void handleNotification(Notification notification, Object handback)
      {
         try
         {
            notificationCount++;
         }
         catch (Exception e)
         {
            fail("Unexpected error: " + e.toString());
         }
      }
   }

   public synchronized void testRegistrationAndUnregistrationNotification() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      MyNotificationListener listener = new MyNotificationListener();
      
      server.addNotificationListener(
            new ObjectName("JMImplementation:type=MBeanServerDelegate"),
            listener, null, null
      );       
    
      // force registration notification
      server.registerMBean(new Test(), new ObjectName("test:foo=bar"));
    
      // force unregistration notification
      server.unregisterMBean(new ObjectName("test:foo=bar"));
      
      // wait for notif to arrive max 5 secs
      for (int i = 0; i < 10; ++i)
      {
         wait(500);
         
         if (listener.notificationCount > 1)
            break;
      }
      
      assertTrue(listener.notificationCount == 2);
   }

}
