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

import junit.framework.Test;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.invoker.CustomClass;
import org.jboss.test.jmx.invoker.InvokerTestMBean;

/**
 * Tests for the jmx invoker adaptor.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
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

   public void testGetSomething()
      throws Exception
   {
      RMIAdaptor server = (RMIAdaptor) getInitialContext().lookup("jmx/invoker/RMIAdaptor");
      assertEquals("something", server.getAttribute(InvokerTestMBean.OBJECT_NAME, "Something"));
   }

   public void testGetCustom()
      throws Exception
   {
      RMIAdaptor server = (RMIAdaptor) getInitialContext().lookup("jmx/invoker/RMIAdaptor");
      assertEquals("InitialValue", ((CustomClass) server.getAttribute(InvokerTestMBean.OBJECT_NAME, "Custom")).getValue());
   }

   public void testSetCustom()
      throws Exception
   {
      RMIAdaptor server = (RMIAdaptor) getInitialContext().lookup("jmx/invoker/RMIAdaptor");
      server.setAttribute(InvokerTestMBean.OBJECT_NAME, new Attribute("Custom", new CustomClass("changed")));
      assertEquals("changed", ((CustomClass) server.getAttribute(InvokerTestMBean.OBJECT_NAME, "Custom")).getValue());
   }
}
