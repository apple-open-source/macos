/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jmx.test;

import javax.management.ObjectName;

import junit.framework.Test;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.system.ServiceMBean;
import org.jboss.test.JBossTestCase;

/**
 * Tests of mbean proxy attributes.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ProxyUnitTestCase
   extends JBossTestCase
{
   public ProxyUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(ProxyUnitTestCase.class, "jmxproxy.sar");
   }

   public void testStarted()
      throws Exception
   {
      // All the tests are done during start service
      RMIAdaptor server = getServer();
      ObjectName serviceName = new ObjectName("jboss.test:name=ProxyTests");
      assertTrue("Proxy tests should be started", server.getAttribute(serviceName, "State").equals(new Integer(ServiceMBean.STARTED)));
      serviceName = new ObjectName("jboss.test:name=ProxyTestsNested");
      assertTrue("Proxy tests nested should be started", server.getAttribute(serviceName, "State").equals(new Integer(ServiceMBean.STARTED)));
      serviceName = new ObjectName("jboss.test:name=ProxyTestsAttribute");
      assertTrue("Proxy tests attribute should be started", server.getAttribute(serviceName, "State").equals(new Integer(ServiceMBean.STARTED)));
   }
}
