/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.stress.timer;

import test.stress.timer.TimerSUITE;

import java.util.ArrayList;
import java.util.Date;
import java.util.Random;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.timer.Timer;
import javax.management.timer.TimerNotification;

import junit.framework.TestCase;

/**
 * Timer Stress Tests
 *
 * This test works by starting a lot of notifications at the start and
 * checks the concurrency by performing lots of operations.<p>
 *
 * It then waits for the slow notifications to complete allowing any lag
 * due to slower computers to be caught up with.<p>
 * 
 * Any concurrency problem or dropped notifications should show up
 * when the test times out and the target notifications are not reached.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class TimerTestCase
   extends TestCase
   implements NotificationListener
{
   // Constants ---------------------------------------------------------------

   String TIMER_TYPE = "TimerType";
   String MESSAGE = "Message";
   String USER_DATA = "UserData";

   // Attributes --------------------------------------------------------------

   /**
    * The MBeanServer
    */
   MBeanServer server;

   /**
    * The object name of the timer
    */
   ObjectName timerName;

   /**
    * The timer
    */
   Timer timer;

   /**
    * The number of notifications
    */
   int notifications = 0;

   /**
    * The target notifications
    */
   int target = 0;

   /**
    * The percentage done
    */
   int nextPercentage = 10;

   // Constructor -------------------------------------------------------------

   /**
    * Construct the test
    */
   public TimerTestCase(String s)
   {
      super(s);
   }

   // Tests -------------------------------------------------------------------

   /**
    * Test the timer under stress conditions
    */
   public void testTortureOne()
      throws Exception
   {
      target = TimerSUITE.TIMERS * TimerSUITE.NOTIFICATIONS;
      System.err.println("Timer Torture One: target=" + target);

      initTest();
      try
      {
         initTimer();
         startTimer();

         // Start lots of timer notifications
         nextPercentage = 10;
         Random random = new Random();
         for (int i = 0; i < TimerSUITE.TIMERS; i++)
         {
            addNotification(TimerSUITE.OFFSET,
                            random.nextInt(TimerSUITE.PERIOD),
                             TimerSUITE.NOTIFICATIONS);
         }

         // Perform some operations
         for (int k = 0; k < target; k++)
         {
            Integer id = addNotification(timer.ONE_HOUR, timer.ONE_HOUR, 1);
            timer.getAllNotificationIDs();
            timer.getDate(id);
            timer.getNbNotifications();;
            timer.getNbOccurences(id);
            timer.getNotificationIDs(TIMER_TYPE);
            timer.getNotificationUserData(id);
            timer.getPeriod(id);
            timer.getSendPastNotifications();
            timer.isActive();
            timer.isEmpty();
            timer.setSendPastNotifications(true);
            removeNotification(id);
         }
         
         // Give it time to complete but check for stalled
         for (int j = 0; j < TimerSUITE.NOTIFICATIONS; j++)
         {
           if (notifications >= target)
              break;
           int lastNotifications = notifications;
           sleep(TimerSUITE.PERIOD * 10);
           if (lastNotifications == notifications)
           {
              sleep(TimerSUITE.PERIOD * 10);
              if (lastNotifications == notifications)
                 break;
           }
         }

         // Test the number of notifications
         assertTrue(notifications == target);
      }   
      finally
      {
         endTest();
      }
   }

   // Support -----------------------------------------------------------------

   /**
    * Start a new test
    */
   private void initTest()
   {
      notifications = 0;
      server = MBeanServerFactory.createMBeanServer();
   }

   /**
    * End the test
    */
   private void endTest()
      throws Exception
   {
      server.removeNotificationListener(timerName, this);
      stopTimer();
      MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Create a timer and register ourselves as a listener
    */
   private void initTimer()
   {
      try
      {
         timer = new Timer();
         timerName = new ObjectName("test:type=timer");
         server.registerMBean(timer, timerName);
         server.addNotificationListener(timerName, this, null, null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }
   }

   /**
    * Start the timer
    */
   private void startTimer()
   {
      timer.start();
   }

   /**
    * Stop the timer, does a small wait to avoid problems with the RI
    */
   private void stopTimer()
   {
      timer.removeAllNotifications();
      timer.stop();
   }

   /**
    * Add a notification
    */
   private Integer addNotification(long offset, long period, long occurs)
   {
      return timer.addNotification(TIMER_TYPE, MESSAGE, USER_DATA,
                                   timeOffset(offset), period, occurs);  
   }

   /**
    * Remove a notification
    */
   private void removeNotification(Integer id)
      throws Exception
   {
      timer.removeNotification(id);
   }

   /**
    * Handle the notification, just add it to the list
    */
   public synchronized void handleNotification(Notification n, Object ignored)
   {
      notifications++;
      float percentage = 100 * notifications / target;
      if (percentage >= nextPercentage)
      {
         System.err.println("Done " + nextPercentage + "%");
         nextPercentage += 10;
      }
   }

   /**
    * Get the time using and offset
    */
   private Date timeOffset(long offset)
   {
      return new Date(System.currentTimeMillis() + offset);
   }

   /**
    * Sleep for a bit
    */
   private void sleep(long time)
   {
      try
      {
         Thread.currentThread().sleep(time);
      }
      catch (InterruptedException ignored)
      {
      }
   }
}
