/* 
 * ====================================================================
 * This is Open Source Software, distributed
 * under the Apache Software License, Version 1.1
 * 
 * 
 *  This software  consists of voluntary contributions made  by many individuals
 *  on  behalf of the Apache Software  Foundation and was  originally created by
 *  Ivelin Ivanov <ivelin@apache.org>. For more  information on the Apache
 *  Software Foundation, please see <http://www.apache.org/>.
 */

package org.jboss.test.ha.jmx.test;

import javax.management.Notification;

import junit.framework.TestCase;

import org.jboss.test.ha.jmx.HAServiceMBeanSupportTester;

/**
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HAServiceMBeanSupportUnitTestCase extends TestCase
{

  private HAServiceMBeanSupportTester haServiceMBeanSupportTester_ = null;

  public HAServiceMBeanSupportUnitTestCase(String name)
  {
    super(name);
  }
   
  public void setUp()
  {
    haServiceMBeanSupportTester_ = new HAServiceMBeanSupportTester();
  }

  
  public void tearDown() 
  {
    haServiceMBeanSupportTester_ = null;
  }


  /**
   * 
   * messages should be sent out to both remote and local listeners.
   *
   */
  public void testSendNotificationBroadcastsToClusterAndLocally()
  {
    Notification notification = new Notification("test.notification", "some:name=tester", 1);
    haServiceMBeanSupportTester_.sendNotification( notification );

    assertEquals("sendNotificationToLocalListeners() was not handed the original notification", 
      haServiceMBeanSupportTester_.__invokationStack__.pop(), notification );

    assertEquals("method not invoked as expected",
      haServiceMBeanSupportTester_.__invokationStack__.pop(), "sendNotificationToLocalListeners");      

    assertEquals("sendNotificationRemote() was not handed the original notification", 
      haServiceMBeanSupportTester_.__invokationStack__.pop(), notification );
    
    assertEquals("method not invoked as expected",
      haServiceMBeanSupportTester_.__invokationStack__.pop(), "sendNotificationRemote");      
  }

  /**
   * 
   * Even if the message cannot be sent out to the cluster,
   * it should still be delivered to local listeners.
   *
   */
  public void testSendNotificationAfterClusterFailureContinueWithLocal()
  {
    haServiceMBeanSupportTester_.__shouldSendNotificationRemoteFail__ = true;

    Notification notification = new Notification("test.notification", "some:name=tester", 1);
    haServiceMBeanSupportTester_.sendNotification( notification );
    
    assertEquals("sendNotificationToLocalListeners() was not handed the original notification", 
    haServiceMBeanSupportTester_.__invokationStack__.pop(), notification );

    assertEquals("method not invoked as expected",
      haServiceMBeanSupportTester_.__invokationStack__.pop(), "sendNotificationToLocalListeners");      
  }

}
