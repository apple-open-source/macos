/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

import javax.management.Notification;
import javax.management.NotificationListener;

/**
 * Simple Listener
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class MBeanListener
   implements NotificationListener, MBeanListenerMBean
{
   public long count = 0;
   public Object source = null;
   public Object handback = null;
   public long count1 = 0;
   public Object source1 = new Object();
   public Object handback1 = new Object();
   public long count2 = 0;
   public Object source2 = new Object();
   public Object handback2 = new Object();

   Object hb1 = null;
   Object hb2 = null;

   public MBeanListener()
   {
   }

   public MBeanListener(String hb1, String hb2)
   {
      this.hb1 = hb1;
      this.hb2 = hb2;
   }

   public void handleNotification(Notification n, Object nhb)
   {
      if (nhb != null && nhb.equals(hb1))
      {
         count1++;
         source1 = n.getSource();
         handback1 = nhb;
      }
      else if (nhb != null && nhb.equals(hb2))
      {
         count2++;
         source2 = n.getSource();
         handback2 = nhb;
      }
      else
      {
         count++;
         source = n.getSource();
         handback = nhb;
      }
   }
}
