/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;

/**
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class Listener
   implements ListenerMBean, NotificationListener
{
   public int notificationCount = 0;

   public boolean error = false;

   public void doSomething()
   {
   }

   public void handleNotification(Notification notification, Object handback)
   {
       if (!(handback instanceof String))
          error = true;
       if (!(handback.equals("MyHandback")))
          error = true;
       if (!(notification.getSource() instanceof ObjectName))
          error = true;
       if (!(notification.getSource().toString().equals("JMImplementation:type=MBeanServerDelegate")))
          error = true;

       notificationCount++;
   }
}
