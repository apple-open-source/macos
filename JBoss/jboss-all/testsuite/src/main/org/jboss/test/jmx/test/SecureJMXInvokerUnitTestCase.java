/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jmx.test;

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import junit.framework.Test;

/** Tests for the jmx invoker adaptor with a secured xmbean.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class SecureJMXInvokerUnitTestCase
   extends JMXInvokerUnitTestCase
{
   public SecureJMXInvokerUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(SecureJMXInvokerUnitTestCase.class, "invoker-adaptor-test.ear");
   }

   ObjectName getObjectName() throws MalformedObjectNameException
   {
      return new ObjectName("jboss.test:service=InvokerTest,secured=true");
   }

}
