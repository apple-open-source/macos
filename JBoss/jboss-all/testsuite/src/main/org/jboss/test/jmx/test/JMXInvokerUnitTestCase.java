/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jmx.test;

import javax.management.Attribute;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import junit.framework.Test;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.invoker.CustomClass;
import org.jboss.test.jmx.invoker.InvokerTestMBean;
import org.jboss.test.jmx.invoker.Listener;

/**
 * Tests for the jmx invoker adaptor.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class JMXInvokerUnitTestCase
   extends JBossTestCase
{
   public JMXInvokerUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(JMXInvokerUnitTestCase.class, "invoker-adaptor-test.ear");
   }

   ObjectName getObjectName() throws MalformedObjectNameException
   {
      return InvokerTestMBean.OBJECT_NAME;
   }

   public void testGetSomething()
      throws Exception
   {
      RMIAdaptor server = (RMIAdaptor) getInitialContext().lookup("jmx/invoker/RMIAdaptor");
      assertEquals("something", server.getAttribute(getObjectName(), "Something"));
   }

   public void testGetCustom()
      throws Exception
   {
      RMIAdaptor server = (RMIAdaptor) getInitialContext().lookup("jmx/invoker/RMIAdaptor");
      CustomClass custom = (CustomClass) server.getAttribute(getObjectName(), "Custom");
      assertEquals("InitialValue", custom.getValue());
   }

   public void testSetCustom()
      throws Exception
   {
      RMIAdaptor server = (RMIAdaptor) getInitialContext().lookup("jmx/invoker/RMIAdaptor");
      server.setAttribute(getObjectName(), new Attribute("Custom", new CustomClass("changed")));
      CustomClass custom = (CustomClass) server.getAttribute(getObjectName(), "Custom");
      assertEquals("changed", custom.getValue());
   }

   /** Test the remoting of JMX Notifications
    * @throws Exception
    */ 
   public void testNotification() throws Exception
   {
      Listener listener = new Listener();
      listener.export();
      RMIAdaptor server = (RMIAdaptor) getInitialContext().lookup("jmx/invoker/RMIAdaptor");
      server.addNotificationListener(getObjectName(), listener, null, "runTimer");
      synchronized( listener )
      {
         listener.wait(15000);
      }
      server.removeNotificationListener(getObjectName(), listener);
      listener.unexport();
      int count = listener.getCount();
      assertTrue("Received 10 notifications, count="+count, count == 10);
   }

}
