/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jmx.test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.ha.HAServiceRemote;

import junit.framework.Test;

/**
 * Tests for ha invoker.
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class HAInvokerUnitTestCase
   extends JBossTestCase
{
   public HAInvokerUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(HAInvokerUnitTestCase.class, "ha-invoker.sar");
   }

   public void testHello()
      throws Exception
   {
      HAServiceRemote remote = (HAServiceRemote) getInitialContext().lookup("jmx/HAService");
      assertEquals("Hello", remote.hello());
   }

}
