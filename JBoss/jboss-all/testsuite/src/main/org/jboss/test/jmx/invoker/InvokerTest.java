/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.invoker;

import java.util.Timer;
import java.util.TimerTask;
import javax.management.ListenerNotFoundException;
import javax.management.NotificationBroadcasterSupport;
import javax.management.NotificationListener;
import javax.management.NotificationFilter;
import javax.management.Notification;
import org.jboss.logging.Logger;

/**
 * Used in JMX invoker adaptor test.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.3 $
 *
 * @jmx:mbean name="jboss.test:service=InvokerTest"
 */
public class InvokerTest
   extends NotificationBroadcasterSupport
   implements InvokerTestMBean
{
   static Logger log = Logger.getLogger(InvokerTest.class);

   private CustomClass custom = new CustomClass("InitialValue");

   /**
    * @jmx:managed-attribute
    */
   public String getSomething()
   {
      return "something";
   }

   public void addNotificationListener(NotificationListener listener,
      NotificationFilter filter, Object handback)
   {
      log.info("addNotificationListener, listener: "+listener+", handback: "+handback);
      super.addNotificationListener(listener, filter, handback);
      if( "runTimer".equals(handback) )
      {
         Timer t = new Timer();
         Send10Notifies task = new Send10Notifies();
         t.scheduleAtFixedRate(task, 0, 1000);
      }
   }

   public void removeNotificationListener(NotificationListener listener)
      throws ListenerNotFoundException
   {
      log.info("removeNotificationListener, listener: "+listener);
      super.removeNotificationListener(listener);
   }

   /**
    * @jmx:managed-attribute
    */
   public CustomClass getCustom()
   {
      return custom;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setCustom(CustomClass custom)
   {
      this.custom = custom;
   }

   /**
    * @jmx:managed-operation
    */
   public CustomClass doSomething(CustomClass custom)
   {
      return new CustomClass(custom.getValue());
   }

   private class Send10Notifies extends TimerTask
   {
      int count;
      /**
       * The action to be performed by this timer task.
       */
      public void run()
      {
         log.info("Sending notification on timer, count="+count);
         Notification notify = new Notification("InvokerTest.timer",
            InvokerTest.this, count);
         InvokerTest.super.sendNotification(notify);
         count ++;
         if( count == 10 )
         {
            super.cancel();
            log.info("Cancelled timer");
         }
      }
   }
}
