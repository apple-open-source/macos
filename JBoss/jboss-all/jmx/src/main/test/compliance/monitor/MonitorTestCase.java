/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.monitor;

import test.compliance.monitor.MonitorSUITE;

import test.compliance.monitor.support.CounterSupport;
import test.compliance.monitor.support.GaugeSupport;
import test.compliance.monitor.support.MonitorSupport;
import test.compliance.monitor.support.StringSupport;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.Iterator;

import javax.management.Attribute;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.monitor.CounterMonitor;
import javax.management.monitor.GaugeMonitor;
import javax.management.monitor.Monitor;
import javax.management.monitor.MonitorNotification;
import javax.management.monitor.StringMonitor;

import junit.framework.AssertionFailedError;
import junit.framework.TestCase;

/**
 * Monitor Notification Tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class MonitorTestCase
   extends TestCase
   implements NotificationListener
{
   // Constants ---------------------------------------------------------------

   // Attributes --------------------------------------------------------------

   /**
    * The MBeanServer
    */
   MBeanServer server;

   /**
    * The object name of the monitor service
    */
   ObjectName monitorName;

   /**
    * The monitor
    */
   Monitor monitor;

   /**
    * The object name of the mbean monitored
    */
   ObjectName monitoredName;

   /**
    * The monitored mbean
    */
   MonitorSupport monitored;

   /**
    * The notifications
    */
   ArrayList notifications = new ArrayList();

   // Constructor -------------------------------------------------------------

   /**
    * Construct the test
    */
   public MonitorTestCase(String s)
   {
      super(s);
   }

   // Tests -------------------------------------------------------------------

   /**
    * Test notification types differ
    */
   public void testNotificationTypes()
   {
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR,
                   "jmx.monitor.error.attribute");
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR,
                   "jmx.monitor.error.type");
      assertEquals(MonitorNotification.OBSERVED_OBJECT_ERROR,
                   "jmx.monitor.error.mbean");
      assertEquals(MonitorNotification.RUNTIME_ERROR,
                   "jmx.monitor.error.runtime");
      assertEquals(MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED,
                   "jmx.monitor.string.differs");
      assertEquals(MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED,
                   "jmx.monitor.string.matches");
      assertEquals(MonitorNotification.THRESHOLD_ERROR,
                   "jmx.monitor.error.threshold");
      assertEquals(MonitorNotification.THRESHOLD_HIGH_VALUE_EXCEEDED,
                   "jmx.monitor.gauge.high");
      assertEquals(MonitorNotification.THRESHOLD_LOW_VALUE_EXCEEDED,
                   "jmx.monitor.gauge.low");
      assertEquals(MonitorNotification.THRESHOLD_VALUE_EXCEEDED,
                   "jmx.monitor.counter.threshold");
   }

   // Counter monitor notification info ---------------------------------------

   /**
    * Test the notification info of the counter
    */
   public void testCounterNotificationInfo()
      throws Exception
   {
      HashSet expected = new HashSet();
      expected.add(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR);
      expected.add(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR);
      expected.add(MonitorNotification.OBSERVED_OBJECT_ERROR);
      expected.add(MonitorNotification.RUNTIME_ERROR);
      expected.add(MonitorNotification.THRESHOLD_ERROR);
      expected.add(MonitorNotification.THRESHOLD_VALUE_EXCEEDED);

      MBeanNotificationInfo[] mbni = new CounterMonitor().getNotificationInfo();
      checkNotificationInfo("Counter", mbni, expected);
   }

   // Counter normal no offset no modulus tests -------------------------------

   /**
    * Test normal counter threshold no offset no modulus
    */
   public void testNormalCounterThresholdExceededEarlyNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(0));
         expectStartMonitor(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset no modulus
    */
   public void testNormalCounterThresholdExceededLateNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset no modulus
    */
   public void testNormalCounterThresholdExceededManyNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(0));
         expect(new Integer(11),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset no modulus
    */
   public void testNormalCounterThresholdNotExceededNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(1));
         dontExpect(new Integer(-1));
         dontExpect(new Integer(9));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(10));
         dontExpect(new Integer(11));
         dontExpect(new Integer(9));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(9));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset no modulus
    */
   public void testNormalCounterThresholdExceededNoneNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            false, new Integer(0), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(10));
         dontExpect(new Integer(0));
         dontExpect(new Integer(10));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter normal offset no modulus tests ----------------------------------

   /**
    * Test normal counter threshold offset no modulus
    */
   public void testNormalCounterThresholdExceededEarlyOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(0));
         expectStartMonitor(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset no modulus
    */
   public void testNormalCounterThresholdExceededLateOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset no modulus
    */
   public void testNormalCounterThresholdExceededManyOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(0));
         dontExpect(new Integer(11));
         expect(new Integer(20),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(29));
         expect(new Integer(30),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expect(new Integer(40),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset no modulus
    */
   public void testNormalCounterThresholdNotExceededOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(1));
         dontExpect(new Integer(-1));
         dontExpect(new Integer(9));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(10));
         dontExpect(new Integer(11));
         dontExpect(new Integer(9));
         expect(new Integer(20),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(19));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset no modulus
    */
   public void testNormalCounterThresholdExceededNoneOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            false, new Integer(10), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(10));
         dontExpect(new Integer(0));
         dontExpect(new Integer(20));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter normal no offset modulus tests ---------------------------------

   /**
    * Test normal counter threshold no offset modulus
    */
   public void testNormalCounterThresholdExceededEarlyNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(10));
         expectStartMonitor(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset modulus
    */
   public void testNormalCounterThresholdExceededLateNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset modulus
    */
   public void testNormalCounterThresholdExceededManyNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(0));
         expect(new Integer(11),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expect(new Integer(12),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      catch (AssertionFailedError e)
      {
         fail("FAILS IN RI: Modulus ignored with no offset???");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset modulus
    */
   public void testNormalCounterThresholdNotExceededNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(0), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(1));
         dontExpect(new Integer(-1));
         dontExpect(new Integer(9));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(9));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(9));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold no offset modulus
    */
   public void testNormalCounterThresholdExceededNoneNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            false, new Integer(0), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(10));
         dontExpect(new Integer(0));
         dontExpect(new Integer(10));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter normal offset modulus tests -------------------------------------

   /**
    * Test normal counter threshold offset modulus
    */
   public void testNormalCounterThresholdExceededEarlyOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         expectStartMonitor(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset modulus
    */
   public void testNormalCounterThresholdExceededLateOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset modulus
    */
   public void testNormalCounterThresholdExceededManyOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(20));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(0));
         dontExpect(new Integer(12));
         expect(new Integer(20),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         if (((CounterMonitor)monitor).getThreshold().equals(new Integer(30)))
           fail("FAILS IN RI: Threshold 10, Offset 10, Modulus 20 should " +
                " never get a threshold of 30");
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(10));
         expect(new Integer(20),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expect(new Integer(20),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expect(new Integer(30),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset modulus
    */
   public void testNormalCounterThresholdNotExceededOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(20));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(1));
         dontExpect(new Integer(-1));
         dontExpect(new Integer(9));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpect(new Integer(10));
         dontExpect(new Integer(11));
         dontExpect(new Integer(9));
         expect(new Integer(20),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         if (((CounterMonitor)monitor).getThreshold().equals(new Integer(30)))
           fail("FAILS IN RI: Threshold 10, Offset 10, Modulus 20 should " +
                " never get a threshold of 30");
         expect(new Integer(19),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test normal counter threshold offset modulus
    */
   public void testNormalCounterThresholdExceededNoneOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            false, new Integer(10), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         dontExpect(new Integer(10));
         dontExpect(new Integer(0));
         dontExpect(new Integer(20));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter difference no offset no modulus tests ---------------------------

   /**
    * Test difference counter threshold no offset no modulus
    */
   public void testDiffCounterThresholdExceededNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(0), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold no offset no modulus
    */
   public void testDiffCounterThresholdExceededManyNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(0), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(0), new Integer(11),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold no offset no modulus
    */
   public void testDiffCounterThresholdNotExceededNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(0), new Integer(0));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(1));
         dontExpectDiff(new Integer(0), new Integer(9));
         dontExpectDiff(new Integer(1), new Integer(10));
         dontExpectDiff(new Integer(9), new Integer(11));
         dontExpectDiff(new Integer(9), new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold no offset no modulus
    */
   public void testDiffCounterThresholdExceededNoneNoOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            true, new Integer(0), new Integer(0));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(10));
         dontExpectDiff(new Integer(0), new Integer(-10));
         dontExpectDiff(new Integer(0), new Integer(100));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter difference offset no modulus tests ------------------------------

   /**
    * Test difference counter threshold offset no modulus
    */
   public void testDiffCounterThresholdExceededOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(10), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold offset no modulus
    */
   public void testDiffCounterThresholdExceededManyOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(10), new Integer(0));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(0), new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(10), new Integer(30),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(30), new Integer(60),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(60), new Integer(100),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(0), new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold offset no modulus
    */
   public void testDiffCounterThresholdNotExceededOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(10), new Integer(0));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(1));
         expectDiff(new Integer(0), new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpectDiff(new Integer(10), new Integer(20));
         expectDiff(new Integer(20), new Integer(40),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         dontExpectDiff(new Integer(40), new Integer(69));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold offset no modulus
    */
   public void testDiffCounterThresholdExceededNoneOffsetNoModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            true, new Integer(10), new Integer(0));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(1));
         dontExpectDiff(new Integer(0), new Integer(10));
         dontExpectDiff(new Integer(10), new Integer(20));
         dontExpectDiff(new Integer(20), new Integer(40));
         dontExpectDiff(new Integer(40), new Integer(69));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter difference no offset modulus tests ------------------------------

   /**
    * Test difference counter threshold no offset modulus
    */
   public void testDiffCounterThresholdExceededNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(0), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold no offset modulus
    */
   public void testDiffCounterThresholdExceededManyNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(0), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(0), new Integer(11),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(12), new Integer(22),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(0), new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold no offset modulus
    */
   public void testDiffCounterThresholdNotExceededNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(0), new Integer(10));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(1));
         dontExpectDiff(new Integer(0), new Integer(9));
         dontExpectDiff(new Integer(11), new Integer(20));
         dontExpectDiffModulus(new Integer(10), new Integer(-3), new Integer(10));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold no offset modulus
    */
   public void testDiffCounterThresholdExceededNoneNoOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            true, new Integer(0), new Integer(10));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(10));
         dontExpectDiffModulus(new Integer(0), new Integer(-10), new Integer(10));
         dontExpectDiff(new Integer(0), new Integer(100));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter difference offset modulus tests ---------------------------------

   /**
    * Test difference counter threshold offset modulus
    */
   public void testDiffCounterThresholdExceededOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(10), new Integer(10));
         dontExpectStartMonitor(new Integer(0));
         expect(new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold offset modulus
    */
   public void testDiffCounterThresholdExceededManyOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(10), new Integer(20));
         dontExpectStartMonitor(new Integer(0));
         expectDiff(new Integer(0), new Integer(11),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(11), new Integer(31),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
         expectDiff(new Integer(0), new Integer(10),
            MonitorNotification.THRESHOLD_VALUE_EXCEEDED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold offset modulus
    */
   public void testDiffCounterThresholdNotExceededOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            true, new Integer(10), new Integer(20));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(1));
         dontExpectDiff(new Integer(1), new Integer(10));
         dontExpectDiffModulus(new Integer(10), new Integer(-13), new Integer(20));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test difference counter threshold offset modulus
    */
   public void testDiffCounterThresholdExceededNoneOffsetModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(false, new Integer(10),
                            true, new Integer(10), new Integer(20));
         dontExpectStartMonitorDiff(new Integer(0), new Integer(10));
         dontExpectDiffModulus(new Integer(0), new Integer(-10), new Integer(20));
         dontExpectDiff(new Integer(0), new Integer(100));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Counter error tests -----------------------------------------------------

   /**
    * Test invalid attribute
    */
   public void testCounterInvalidAttribute()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         attributeErrorStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute
    */
   public void testCounterInvalidAttributeNull()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         attributeNullStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute type
    */
   public void testCounterInvalidAttributeType()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         attributeTypeStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test write only attribute
    */
   public void testCounterWriteOnly()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         attributeWriteStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute type
    */
   public void testCounterInvalidObjectName()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         objectNameStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid threshold
    */
   public void testCounterInvalidThreshold()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Long(10),
                            false, new Integer(10), new Integer(10));
         objectNameStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid threshold
    */
   public void testCounterInvalidOffset()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Long(10), new Integer(10));
         objectNameStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid threshold
    */
   public void testCounterInvalidModulus()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Long(10));
         objectNameStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test runtime error
    */
   public void testCounterRuntimeError()
      throws Exception
   {
      initTest();
      try
      {
         initCounterMonitor(true, new Integer(10),
                            false, new Integer(10), new Integer(10));
         runtimeErrorStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Gauge notification tests ------------------------------------------------

   /**
    * Test the notification info of the gauge
    */
   public void testGaugeNotificationInfo()
      throws Exception
   {
      HashSet expected = new HashSet();
      expected.add(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR);
      expected.add(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR);
      expected.add(MonitorNotification.OBSERVED_OBJECT_ERROR);
      expected.add(MonitorNotification.RUNTIME_ERROR);
      expected.add(MonitorNotification.THRESHOLD_ERROR);
      expected.add(MonitorNotification.THRESHOLD_HIGH_VALUE_EXCEEDED);
      expected.add(MonitorNotification.THRESHOLD_LOW_VALUE_EXCEEDED);

      MBeanNotificationInfo[] mbni = new GaugeMonitor().getNotificationInfo();
      checkNotificationInfo("Gauge", mbni, expected);
   }

   // Gauge error tests -------------------------------------------------------

   /**
    * Test invalid attribute
    */
   public void testGaugeInvalidAttribute()
      throws Exception
   {
      initTest();
      try
      {
         initGaugeMonitor(true, true, new Integer(10), new Integer(0), false);
         attributeErrorStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute
    */
   public void testGaugeInvalidAttributeNull()
      throws Exception
   {
      initTest();
      try
      {
         initGaugeMonitor(true, true, new Integer(10), new Integer(0), false);
         attributeNullStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute type
    */
   public void testGaugeInvalidAttributeType()
      throws Exception
   {
      initTest();
      try
      {
         initGaugeMonitor(true, true, new Integer(10), new Integer(0), false);
         attributeTypeStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test write only
    */
   public void testGaugeWriteOnly()
      throws Exception
   {
      initTest();
      try
      {
         initGaugeMonitor(true, true, new Integer(10), new Integer(0), false);
         attributeWriteStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute type
    */
   public void testGaugeInvalidObjectName()
      throws Exception
   {
      initTest();
      try
      {
         initGaugeMonitor(true, true, new Integer(10), new Integer(0), false);
         objectNameStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid threshold
    */
   public void testGaugeInvalidThreshold()
      throws Exception
   {
      initTest();
      try
      {
         initGaugeMonitor(true, true, new Long(10), new Long(0), false);
         objectNameStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test runtime error
    */
   public void testGaugeRuntimeError()
      throws Exception
   {
      initTest();
      try
      {
         initGaugeMonitor(true, true, new Integer(10), new Integer(0), false);
         runtimeErrorStartMonitor(new Integer(0));
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // String notification tests -----------------------------------------------

   /**
    * Test the notification info of the string
    */
   public void testStringNotificationInfo()
      throws Exception
   {
      HashSet expected = new HashSet();
      expected.add(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR);
      expected.add(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR);
      expected.add(MonitorNotification.OBSERVED_OBJECT_ERROR);
      expected.add(MonitorNotification.RUNTIME_ERROR);
      expected.add(MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
      expected.add(MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);

      MBeanNotificationInfo[] mbni = new StringMonitor().getNotificationInfo();
      checkNotificationInfo("String", mbni, expected);
   }

   // String test -------------------------------------------------------------

   /**
    * Test a string differs
    */
   public void testStringDifferEarly()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, false, "Hello");
         expectStartMonitor("Goodbye", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string differs
    */
   public void testStringDifferLate()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, false, "Hello");
         dontExpectStartMonitor("Hello");
         expect("Goodbye",
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string differs many
    */
   public void testStringDifferManyEarly()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, false, "Hello");
         expectStartMonitor("Goodbye",
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         dontExpect("Hello");
         expect("Goodbye",
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         dontExpect("Hello");
         expect("Goodbye",
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         dontExpect("Hello");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string differs many
    */
   public void testStringDifferManyLate()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, false, "Hello");
         dontExpectStartMonitor("Hello");
         expect("Goodbye",
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         dontExpect("Hello");
         expect("Goodbye",
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         dontExpect("Hello");
         expect("Goodbye",
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         dontExpect("Hello");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string matches
    */
   public void testStringMatchEarly()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(false, true, "Hello");
         expectStartMonitor("Hello", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string matches
    */
   public void testStringMatchLate()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(false, true, "Hello");
         dontExpectStartMonitor("Goodbye");
         expect("Hello",
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string matches
    */
   public void testStringMatchManyEarly()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(false, true, "Hello");
         expectStartMonitor("Hello",
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
         dontExpect("Goodbye");
         expect("Hello",
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
         dontExpect("Goodbye");
         expect("Hello",
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string matches
    */
   public void testStringMatchManyLate()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(false, true, "Hello");
         dontExpectStartMonitor("Goodbye");
         expect("Hello",
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
         dontExpect("Goodbye");
         expect("Hello",
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
         dontExpect("Goodbye");
         expect("Hello",
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string differs and matches
    */
   public void testStringBoth()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         expectStartMonitor("Goodbye", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         expect("Hello", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string differs and matches
    */
   public void testStringBothMany()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         expectStartMonitor("Goodbye", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         expect("Hello", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
         expect("Goodbye", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
         expect("Hello", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED);
         expect("Goodbye", 
            MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED);
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test a string nothing
    */
   public void testStringNever()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(false, false, "Hello");
         dontExpectStartMonitor("Goodbye");
         dontExpect("Hello");
         dontExpect("Goodbye");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // String error tests ------------------------------------------------------

   /**
    * Test invalid attribute
    */
   public void testStringInvalidAttribute()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         attributeErrorStartMonitor("Goodbye");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute
    */
   public void testStringInvalidAttributeNull()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         attributeNullStartMonitor("Goodbye");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute type
    */
   public void testStringInvalidAttributeType()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         attributeTypeStartMonitor("Goodbye");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test write only attribute
    */
   public void testStringWriteOnly()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         attributeWriteStartMonitor("Goodbye");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test invalid attribute type
    */
   public void testStringInvalidObjectName()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         objectNameStartMonitor("Goodbye");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test runtime error
    */
   public void testStringRuntimeError()
      throws Exception
   {
      initTest();
      try
      {
         initStringMonitor(true, true, "Hello");
         runtimeErrorStartMonitor("Goodbye");
      }
      finally
      {
         stopMonitor();
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Support -----------------------------------------------------------------

   /**
    * Create a counter monitor
    */
   private void initCounterMonitor(boolean notify, Number threshold,
        boolean differenceMode, Number offset, Number modulus)
   {
      try
      {
         CounterMonitor counterMonitor = new CounterMonitor();
         counterMonitor.setNotify(notify);
         counterMonitor.setThreshold(threshold);
         counterMonitor.setDifferenceMode(differenceMode);
         counterMonitor.setOffset(offset);
         counterMonitor.setModulus(modulus);
         CounterSupport support = new CounterSupport();
         monitor = counterMonitor;
         monitored = support;
         initMonitor();
      }
      catch (Exception e)
      {
         fail(e.toString());
      }
   }

   /**
    * Check a counter's threshold
    */
   private void checkThreshold(Number number)
   {
      assertEquals(number, ((CounterMonitor)monitor).getThreshold());
   }

   /**
    * Create a gauge monitor
    */
   private void initGaugeMonitor(boolean notifyHigh, boolean notifyLow, 
        Number thresholdHigh, Number thresholdLow, boolean differenceMode)
   {
      try
      {
         GaugeMonitor gaugeMonitor = new GaugeMonitor();
         gaugeMonitor.setNotifyHigh(notifyHigh);
         gaugeMonitor.setNotifyLow(notifyLow);
         gaugeMonitor.setThresholds(thresholdHigh, thresholdLow);
         gaugeMonitor.setDifferenceMode(differenceMode);
         GaugeSupport support = new GaugeSupport();
         monitor = gaugeMonitor;
         monitored = support;
         initMonitor();
      }
      catch (Exception e)
      {
         fail(e.toString());
      }
   }

   /**
    * Create a string monitor
    */
   private void initStringMonitor(boolean differ, boolean match, String compare)
   {
      try
      {
         StringMonitor stringMonitor = new StringMonitor();
         stringMonitor.setNotifyDiffer(differ);
         stringMonitor.setNotifyMatch(match);
         stringMonitor.setStringToCompare(compare);
         StringSupport support = new StringSupport();
         monitor = stringMonitor;
         monitored = support;
         initMonitor();
      }
      catch (Exception e)
      {
         fail(e.toString());
      }
   }

   /**
    * Start a new test
    */
   private void initTest()
   {
      notifications.clear();
      server = MBeanServerFactory.createMBeanServer();
   }

   /**
    * Create the monitor
    */
   private void initMonitor() throws Exception
   {
      monitorName = new ObjectName("test:type=monitor");
      monitoredName = new ObjectName("test:type=monitored");
      monitor.setObservedObject(monitoredName);
      monitor.setObservedAttribute("Value");
      monitor.setGranularityPeriod(MonitorSUITE.GRANULARITY_TIME);
      server.registerMBean(monitor, monitorName);
      server.registerMBean(monitored, monitoredName);
      server.addNotificationListener(monitorName, this, null, null);
   }

   /**
    * Stop the monitor
    */
   private void stopMonitor()
   {
      if (monitor != null)
      {
         monitored.end();
         monitor.stop();
      }
   }

   /**
    * Handle the notification, just add it to the list
    */
   public void handleNotification(Notification n, Object ignored)
   {
      synchronized(notifications)
      {
         notifications.add(n);
         notifications.notifyAll();
      }
   }

   /**
    * Sync with the notification handler
    */
   private void sync(boolean getWorks)
      throws Exception
   {
      // Make sure the monitor has got the attribute
      if (getWorks)
         waitCycle();
      else
      {
         synchronized(notifications)
         {
            notifications.wait(MonitorSUITE.MAX_WAIT);
         }
      }
   }

   /**
    * Invalid attribute start monitor
    */
   private void attributeErrorStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.setObservedAttribute("rubbish");
      monitor.start();
      sync(false);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("rubbish", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR, n.getType());
      n = serializeDeserialize(n);
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("rubbish", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR, n.getType());
   }

   /**
    * Invalid attribute null start monitor
    */
   private void attributeNullStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.setObservedAttribute("WrongNull");
      monitor.start();
      sync(false);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WrongNull", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR,
                   n.getType());
      n = serializeDeserialize(n);
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WrongNull", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR,
                   n.getType());
   }

   /**
    * Invalid attribute type start monitor
    */
   private void attributeTypeStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.setObservedAttribute("WrongType");
      monitor.start();
      sync(false);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WrongType", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR,
                   n.getType());
      n = serializeDeserialize(n);
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WrongType", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR,
                   n.getType());
   }

   /**
    * Write only Attribute start monitor
    */
   private void attributeWriteStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.setObservedAttribute("WriteOnly");
      monitor.start();
      sync(false);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WriteOnly", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR,
                   n.getType());
      n = serializeDeserialize(n);
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WriteOnly", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR,
                   n.getType());
   }

   /**
    * Invalid objectName start monitor
    */
   private void objectNameStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitoredName = new ObjectName("rubbish:type=pants");
      monitor.setObservedObject(monitoredName);
      monitor.start();
      sync(false);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_OBJECT_ERROR,
                   n.getType());
      n = serializeDeserialize(n);
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(MonitorNotification.OBSERVED_OBJECT_ERROR,
                   n.getType());
   }

   /**
    * Invalid threshold start monitor
    */
   private void thresholdStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.start();
      sync(false);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(MonitorNotification.THRESHOLD_ERROR,
                   n.getType());
      n = serializeDeserialize(n);
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(MonitorNotification.THRESHOLD_ERROR,
                   n.getType());
   }

   /**
    * runtime error start monitor
    */
   private void runtimeErrorStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.setObservedAttribute("WrongException");
      monitor.start();
      sync(false);
      if (notifications.size() != 1)
         fail ("FAILS IN RI: Does not notify of error thrown by getter");
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WrongException", n.getObservedAttribute());
      assertEquals(MonitorNotification.RUNTIME_ERROR,
                   n.getType());
      n = serializeDeserialize(n);
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("WrongException", n.getObservedAttribute());
      assertEquals(MonitorNotification.RUNTIME_ERROR,
                   n.getType());
   }

   /**
    * Start a monitor expect a notification
    */
   private void expectStartMonitor(Object expected, String type)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.start();
      setValue(expected);

      // Synchronize the notification
      sync(true);

      // Check the result
      checkGauge(expected);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(type, n.getType());
      assertEquals(expected, n.getDerivedGauge());
      n = serializeDeserialize(n);
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(type, n.getType());
      assertEquals(expected, n.getDerivedGauge());
   }

   /**
    * Start a monitor, don't expect a notification
    */
   private void dontExpectStartMonitor(Object initial)
      throws Exception
   {
      assertEquals(0, notifications.size());
      monitor.start();
      setValue(initial);
      waitCycle();
      assertEquals(0, notifications.size());
      checkGauge(initial);
   }

   /**
    * Start a monitor, don't expect a notification difference
    */
   private void dontExpectStartMonitorDiff(Object value1, Object value2)
      throws Exception
   {
      monitor.start();
      setValue(value1);
      waitCycle();
      assertEquals(0, notifications.size());
      setValue(value2);
      waitCycle();
      assertEquals(0, notifications.size());
      checkGauge(sub(value2, value1));
   }

   private void setValue(Object value)
      throws Exception
   {
      monitored.lock("set");
      server.setAttribute(monitoredName, new Attribute("Value", value));
   }

   /**
    * Expect a notification
    */
   private void expect(Object expected, Object gauge, String type)
      throws Exception
   {
      assertEquals(0, notifications.size());
      setValue(expected);

      // Synchronize the notification
      sync(true);

      // Check the result
      checkGauge(gauge);
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(type, n.getType());
      assertEquals(gauge, n.getDerivedGauge());
   }

   /**
    * Expect a notification difference
    */
   private void expectDiff(Object value1, Object value2, String type)
      throws Exception
   {
      waitCycle();
      notifications.clear();
      setValue(value1);
      waitCycle();
      assertEquals(0, notifications.size());
      setValue(value2);
      sync(true);

      // Check the result
      checkGauge(sub(value2, value1));
      assertEquals(1, notifications.size());
      MonitorNotification n = (MonitorNotification) notifications.get(0);
      notifications.clear();
      assertEquals(monitorName, n.getSource());
      assertEquals(monitoredName, n.getObservedObject());
      assertEquals("Value", n.getObservedAttribute());
      assertEquals(type, n.getType());
      assertEquals(sub(value2, value1), n.getDerivedGauge());
   }

   /**
    * Expect a notification
    */
   private void expect(Object expected, String type)
      throws Exception
   {
      expect(expected, expected, type);
   }

   /**
    * Don't expect a notification
    */
   private void dontExpect(Object unexpected)
      throws Exception
   {
      assertEquals(0, notifications.size());
      setValue(unexpected);
      waitCycle();
      assertEquals(0, notifications.size());
      checkGauge(unexpected);
   }

   /**
    * Don't expect a notification
    */
   private void dontExpectDiff(Object value1, Object value2)
      throws Exception
   {
      waitCycle();
      notifications.clear();
      setValue(value1);
      waitCycle();
      assertEquals(0, notifications.size());
      setValue(value2);
      waitCycle();
      assertEquals(0, notifications.size());
      checkGauge(sub(value2, value1));
   }

   /**
    * Don't expect a notification
    */
   private void dontExpectDiffModulus(Object value1, Object value2, Object value3)
      throws Exception
   {
      waitCycle();
      notifications.clear();
      setValue(value1);
      waitCycle();
      assertEquals(0, notifications.size());
      setValue(value2);
      waitCycle();
      assertEquals(0, notifications.size());
      checkGauge(add(sub(value2, value1), value3));
   }

   /**
    * Check the gauge
    */
  private void checkGauge(Object gauge)
     throws Exception
  {
      if (monitor instanceof StringMonitor)
         assertEquals(gauge, ((StringMonitor)monitor).getDerivedGauge());
      else if (monitor instanceof CounterMonitor)
         assertEquals(gauge, ((CounterMonitor)monitor).getDerivedGauge());
      else if (monitor instanceof GaugeMonitor)
         assertEquals(gauge, ((GaugeMonitor)monitor).getDerivedGauge());
      else
         fail("You idiot!");
  }

   /**
    * Wait one cycle of the monitor
    */
   private void waitCycle()
   {
      monitored.unlock("set");
      monitored.lock("set");
   }

  /**
   * Add two numbers.
   * @param value1 the first value.
   * @param value2 the second value.
   * @return value1 + value2 of the correct type
   */
  private Number add(Object value1, Object value2)
  {
     if (value1 instanceof Byte)
       return new Byte((byte) (((Byte)value1).byteValue() + ((Byte)value2).byteValue()));
     if (value1 instanceof Integer)
       return new Integer(((Integer)value1).intValue() + ((Integer)value2).intValue());
     if (value1 instanceof Short)
       return new Short((short) (((Short)value1).shortValue() + ((Short)value2).shortValue()));
     if (value1 instanceof Long)
       return new Long(((Long)value1).longValue() + ((Long)value2).longValue());
     fail("You idiot!");
     return null;
  }

  /**
   * Subtract two numbers.
   * @param value1 the first value.
   * @param value2 the second value.
   * @return value1 - value2 of the correct type
   */
  private Number sub(Object value1, Object value2)
  {
     if (value1 instanceof Byte)
       return new Byte((byte) (((Byte)value1).byteValue() - ((Byte)value2).byteValue()));
     if (value1 instanceof Integer)
       return new Integer(((Integer)value1).intValue() - ((Integer)value2).intValue());
     if (value1 instanceof Short)
       return new Short((short) (((Short)value1).shortValue() - ((Short)value2).shortValue()));
     if (value1 instanceof Long)
       return new Long(((Long)value1).longValue() - ((Long)value2).longValue());
     fail("You idiot!");
     return null;
  }

  /**
   * Check the notification info is as expected
   *
   * @param mbni the received notification info
   * @param expected the expected notifications
   */
  private void checkNotificationInfo(String type,
                                     MBeanNotificationInfo[] mbni,
                                     HashSet expected)
  {
     String[] types = mbni[0].getNotifTypes();
     for (int i = 0; i < types.length; i++)
         if (expected.remove(types[i]) == false)
            fail(type + ": didn't expect notification type " + types[i]);
     Iterator iterator = expected.iterator();
     while (iterator.hasNext())
         fail(type + ": expected notification type " + iterator.next());
  }

  /**
   * Serialize/Derserialize the notification
   *
   * @param n the notification
   * @return the notification after serialize/deserialze
   */
  private MonitorNotification serializeDeserialize(MonitorNotification n)
     throws Exception
  {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(n);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      return (MonitorNotification) ois.readObject();
  }
}
