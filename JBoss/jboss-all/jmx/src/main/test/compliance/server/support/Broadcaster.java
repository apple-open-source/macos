/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

import javax.management.Notification;
import javax.management.NotificationBroadcasterSupport;

/**
 * Simple Broadcaster
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 *   
 */
public class Broadcaster
   extends NotificationBroadcasterSupport
   implements BroadcasterMBean
{
   long sequence = 0;
   public void doSomething()
   {
      sendNotification(new Notification("test", this, ++sequence));
   }
}
