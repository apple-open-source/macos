
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.test;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.interfaces.UnshareableConnectionSessionHome;
import org.jboss.test.jca.interfaces.UnshareableConnectionSession;
import javax.management.ObjectName;
import javax.management.Attribute;

/**
 * Tests unshared connections
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 */

public class UnsharedConnectionUnitTestCase extends JBossTestCase
{

   private UnshareableConnectionSessionHome home;
   private UnshareableConnectionSession bean;

   public UnsharedConnectionUnitTestCase (String name)
   {
      super(name);
   }

   protected void setSpecCompliant(Boolean value)
      throws Exception
   {
      ObjectName CCM = new ObjectName("jboss.jca:service=CachedConnectionManager");
      getServer().setAttribute(CCM, new Attribute("SpecCompliant", value));
   }

   protected void setUp() throws Exception
   {
      setSpecCompliant(Boolean.TRUE);
      home = (UnshareableConnectionSessionHome)getInitialContext().lookup("UnshareableStateless");
      bean = home.create();
   }

   protected void tearDown() throws Exception
   {
      setSpecCompliant(Boolean.FALSE);
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(UnsharedConnectionUnitTestCase.class, "jcatest-unshared.jar");
   }

   public void testUnsharedConnection() throws Exception
   {
      bean.runTest();
   }
}
