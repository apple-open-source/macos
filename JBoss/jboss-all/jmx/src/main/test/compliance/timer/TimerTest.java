/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.timer;

import junit.framework.TestCase;

import java.util.ArrayList;
import java.util.Date;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.timer.TimerNotification;

/**
 * Basic timer test.<p>
 *
 * The aim of these tests is to check the most common uses of the timer
 * service.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class TimerTest
  extends TestCase
  implements NotificationListener
{
/**
 * The period for a timer notification. This needs to be small so the tests * don't take too long. * The wait needs to be long enough to be sure the monitor has enough time * to send the notification and switch the context to the handler. */ 
public static final long PERIOD = 100; 
public static final long WAIT = 10000; 
/**
 * The number of repeats for occurances tests */
 public static final long REPEATS = 2; 

  // Attributes ----------------------------------------------------------------

  /**
   * The object name of the timer service
   */
  ObjectName timerName;

  /**
   * The MBean server
   */
  MBeanServer server;

  /**
   * The received notifications
   */
  ArrayList receivedNotifications = new ArrayList();

  // Constructor ---------------------------------------------------------------

  public TimerTest(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Test a single notification works.
   */
  public void testSingleNotification()
     throws Exception
  {
    try
    {
      startTimerService();

      Integer id = addNotification("test", "hello", "data", calcTime(PERIOD),
                                   0, 1);
      expectNotifications(1);

// TODO
//      if (getNotificationType(id) != null)
//        fail("Single notification still registered");
    }
    finally
    {
      stopTimerService();
    }
  }

  /**
   * Test a repeated notification works.
   */
  public void testRepeatedNotification()
     throws Exception
  {
    try
    {
      startTimerService();
      Integer id = addNotification("test", "hello", "data", calcTime(PERIOD),
                                   PERIOD, REPEATS);
      expectNotifications(1);
      expectNotifications(2);

// TODO
//      if (getNotificationType(id) != null)
//        fail("Repeated notification still registered");
    }
    finally
    {
      stopTimerService();
    }
  }

  /**
   * Test infinite notification works.
   */
  public void testInfiniteNotification()
     throws Exception
  {
    try
    {
      startTimerService();

      Integer id = addNotification("test", "hello", "data", calcTime(PERIOD),
                                   PERIOD, 0);
      expectNotifications(1);
      expectNotifications(2);

      if (getNotificationType(id) == null)
        fail("Infinite notification not registered");
    }
    finally
    {
      stopTimerService();
    }
  }

   /**
    * Test two infinite notification works.
    */
   public void testTwoNotificationProducers()
      throws Exception
   {
      try
      {
         startTimerService();
         long lTimeOne = 5 * 1000;
         long lTimeTwo = 12 * 1000;
         long lWait = 2 * lTimeTwo;
         long lStart = calcTime( lTimeOne );
         Integer lIdOne = addNotification( "test-2", "hello", "data", lStart + lTimeOne,
                                           lTimeOne, 0);
         Integer lIdTwo = addNotification( "test-2", "hello", "data", lStart + lTimeTwo,
                                           lTimeTwo, 0);
         
         expectNotifications( 1, lWait );
         expectNotifications( 2, lWait );
         // Check time differences which should be around TIME ONE
         TimerNotification lNotificationOne = (TimerNotification) receivedNotifications.get( 0 );
         TimerNotification lNotificationTwo = (TimerNotification) receivedNotifications.get( 1 );
         checkNotificationID( lNotificationOne, lIdOne );
         checkNotificationID( lNotificationTwo, lIdOne );
         checkTimeDifference( lNotificationOne, lNotificationTwo, lTimeOne );
         
         expectNotifications( 3, lWait );
         lNotificationOne = lNotificationTwo;
         lNotificationTwo = (TimerNotification) receivedNotifications.get( 2 );
         checkNotificationID( lNotificationTwo, lIdTwo );
         checkTimeDifference( lNotificationOne, lNotificationTwo, ( lTimeTwo - ( 2 * lTimeOne ) ) );
         
         expectNotifications( 4, lWait );
         lNotificationOne = lNotificationTwo;
         lNotificationTwo = (TimerNotification) receivedNotifications.get( 3 );
         checkNotificationID( lNotificationTwo, lIdOne );
         checkTimeDifference( lNotificationOne, lNotificationTwo, ( ( 3 * lTimeOne ) - lTimeTwo ) );
         
         expectNotifications( 5, lWait );
         lNotificationOne = lNotificationTwo;
         lNotificationTwo = (TimerNotification) receivedNotifications.get( 4 );
         checkNotificationID( lNotificationTwo, lIdOne );
         checkTimeDifference( lNotificationOne, lNotificationTwo, lTimeOne );
         
         expectNotifications( 6, lWait );
         lNotificationOne = lNotificationTwo;
         lNotificationTwo = (TimerNotification) receivedNotifications.get( 5 );
         checkNotificationID( lNotificationTwo, lIdTwo );
         checkTimeDifference( lNotificationOne, lNotificationTwo, ( ( 2 * lTimeTwo ) - ( 4 * lTimeOne ) ) );
      }
      finally
      {
         stopTimerService();
      }
   }

  // Support functions ---------------------------------------------------------

  /**
   * Get an MBeanServer, install the timer service and a notification
   * listener.
   */
  private void startTimerService()
    throws Exception
  {
    server = MBeanServerFactory.createMBeanServer("Timer");

    timerName = new ObjectName("Timer:type=TimerService");
    server.createMBean("javax.management.timer.Timer", timerName,
                       new Object[0], new String[0]);
    server.invoke(timerName, "start", new Object[0], new String[0]);

    receivedNotifications.clear();
    server.addNotificationListener(timerName, this, null, null);
  }

  /**
   * Remove everything used by this test. Cannot report failures because
   * the test might have failed earlier. All notifications are removed,
   * the RI hangs otherwise.
   */
  private void stopTimerService()
  {
    try
    {
      server.invoke(timerName, "removeAllNotifications", new Object[0], new String[0]);
      server.invoke(timerName, "stop", new Object[0], new String[0]);
      server.unregisterMBean(timerName);
      MBeanServerFactory.releaseMBeanServer(server);
    }
    catch (Exception ignored) {}
  }

  /**
   * Handle a notification, just add it to the list
   *
   * @param notification the notification received
   * @param handback not used
   */
  public void handleNotification(Notification notification, Object handback)
  {
    synchronized (receivedNotifications)
    {
      receivedNotifications.add(notification);
      receivedNotifications.notifyAll();
    }
  }

  /**
   * Wait for the timer notification and see if we have the correct number
   * hopefully this should synchronize this test with the timer thread.
   *
   * @param expected the number of notifications expected
   * @throws Exception when the notifications are incorrect
   */
  public void expectNotifications(int expected)
    throws Exception
  {
     expectNotifications( expected, WAIT );
  }

  /**
   * Wait for the timer notification and see if we have the correct number
   * hopefully this should synchronize this test with the timer thread.
   *
   * @param expected the number of notifications expected
   * @param wait time in milli seconds to wait for the notification
   * @throws Exception when the notifications are incorrect
   */
  public void expectNotifications(int expected, long wait)
    throws Exception
  {
     synchronized (receivedNotifications)
     {
       if (receivedNotifications.size() > expected)
         fail("too many notifications");
       if (receivedNotifications.size() < expected)
       {
         receivedNotifications.wait( wait );
       }
       assertEquals(expected, receivedNotifications.size());
     }
  }

  /**
   * Checks if the given Notification ID is the same as the
   * one of the given Notification
   *
   * @param pNotification Notification to be tested
   * @param pNotificationID Id the Notification should have
   **/
   public void checkNotificationID( TimerNotification pNotification, Integer pNotificationID ) {
      if( pNotification == null ) {
         fail( "Notification is null" );
      }
      if( !pNotification.getNotificationID().equals( pNotificationID ) ) {
         fail( "Wrong Notification ID received: " + pNotification.getNotificationID() +
            ", expected: " + pNotificationID );
      }
  }
  
  /**
   * Checks if the time between the two Notification is in a
   * +- 10% limit
   *
   * @param pNotificationOne First Notification to be tested
   * @param pNotificationTwo Second Notification to be tested
   * @param pTimeDiffernce Expected Time Difference
   **/
   public void checkTimeDifference(
      TimerNotification pNotificationOne,
      TimerNotification pNotificationTwo,
      long pTimeDiffernce
   ) {
      long lDiff = pNotificationTwo.getTimeStamp() - pNotificationOne.getTimeStamp();
      if( lDiff < ( pTimeDiffernce - ( pTimeDiffernce / 10 ) ) ||
         lDiff > ( pTimeDiffernce + ( pTimeDiffernce / 10 ) )
      ) {
         fail( "Time between first two notification is too small or too big: " + pTimeDiffernce );
      }
   }

  /**
   * Add a timer notification
   *
   * @param type the type of the notification
   * @param message the message
   * @param data the user data
   * @param time the time of the notification
   * @param period the period of notification
   * @param occurs the number of occurances
   * @return the id of the notfication
   */
  private Integer addNotification(String type, String message, String data,
                                  long time, long period, long occurs)
    throws Exception
  {
    return (Integer) server.invoke(timerName, "addNotification",
      new Object[] { type, message, data, new Date(time), new Long(period), 
                     new Long(occurs) },
      new String[] { "java.lang.String", "java.lang.String", "java.lang.Object",
                     "java.util.Date", "long", "long" } );
  }

  /**
   * Get the notification type for an id
   *
   * @param id the id of the notification
   * @return the type of the notification
   */
  private String getNotificationType(Integer id)
    throws Exception
  {
    // This is called after the last expected notification
    // The timer thread has notified us, but hasn't had time
    // to remove the notification, give it chance, before
    // checking for correct behaviour.
    Thread.yield();
    
    return (String) server.invoke(timerName, "getNotificationType",
      new Object[] { id },
      new String[] { "java.lang.Integer" });
  }

  /**
   * Calculate the time using an offset from the current time.
   * @param offset the offset from the current time
   * @return the calculated time
   */
  private long calcTime(long offset)
  {
    return System.currentTimeMillis() + offset;
  }
}
