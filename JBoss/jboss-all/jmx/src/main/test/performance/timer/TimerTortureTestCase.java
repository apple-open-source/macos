/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.performance.timer;

import test.performance.PerformanceSUITE;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import java.util.ArrayList;
import java.util.Date;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.timer.Timer;
import javax.management.timer.TimerNotification;

import junit.framework.TestCase;

/**
 * Timer Performance Tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class TimerTortureTestCase
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
    * The timer notification id
    */
   Integer id;

   /**
    * The target notifications
    */
   int target = 0;

   /**
    * The number of notifications
    */
   int notifications = 0;

   // Constructor -------------------------------------------------------------

   /**
    * Construct the test
    */
   public TimerTortureTestCase(String s)
   {
      super(s);
   }

   // Tests -------------------------------------------------------------------

   /**
    * See how long to do many notifications one notification, 
    * this tests the overhead
    */
   public void testTortureOne()
   {
      System.err.println("\nTimer iterations " + PerformanceSUITE.TIMER_ITERATION_COUNT);
      System.err.println("One notification at 1 millsecond intervals.");
      initTest();
      try
      {
         initTimer();
         startTimer();
         target = PerformanceSUITE.TIMER_ITERATION_COUNT;
         long start = timeOffset(0).getTime();
         addNotification(1000, 1, PerformanceSUITE.TIMER_ITERATION_COUNT);
         sync();
         sleep(1000);
         long end = timeOffset(0).getTime();
         stopTimer();

         System.err.println("Time (ms): " + (end-start));
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * See how long to many notifications one notification, this tests the overhead
    */
   public void testTortureTen()
   {
      System.err.println("\nTimer iterations " + PerformanceSUITE.TIMER_ITERATION_COUNT);
      System.err.println("Ten notifications at 1 millsecond intervals.");
      initTest();
      try
      {
         initTimer();
         startTimer();
         target = 10 * PerformanceSUITE.TIMER_ITERATION_COUNT;
         long start = timeOffset(0).getTime();
         for (int i=0; i<10; i++)
            addNotification(1000, 1, PerformanceSUITE.TIMER_ITERATION_COUNT);
         sync();
         sleep(1000);
         long end = timeOffset(0).getTime();
         stopTimer();

         System.err.println("Time (ms): " + (end-start));
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
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
      timer.stop();
   }

   /**
    * Add a notification
    */
   private void addNotification(long offset, long period, long occurs)
   {
      id = timer.addNotification(TIMER_TYPE, MESSAGE, USER_DATA,
                                 timeOffset(offset), period, occurs);  
   }

   /**
    * Handle the notification, just add it to the list
    */
   public void handleNotification(Notification n, Object ignored)
   {
      notifications++;
      TimerNotification tn = (TimerNotification) n;
      if (timer.getNbOccurences(tn.getNotificationID()).longValue() == 1)
         synchronized(timerName)
         {
            timerName.notifyAll();
         }
   }

   /**
    * Sync with the notification handler
    */
   private void sync()
   {
      synchronized(timerName)
      {
         try
         {
            timerName.wait(60000);
         }
         catch (InterruptedException ignored)
         {
         }
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
