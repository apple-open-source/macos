
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
import org.jboss.test.jca.interfaces.CachedConnectionSessionHome;
import org.jboss.test.jca.interfaces.CachedConnectionSession;
import javax.management.ObjectName;
import javax.management.Attribute;

/**
 * CachedConnectionSessionUnitTestCase.java
 * Tests connection disconnect-reconnect mechanism.
 *
 * Created: Fri Mar 15 22:48:41 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class CachedConnectionSessionUnitTestCase extends JBossTestCase
{

   private CachedConnectionSessionHome sh;
   private CachedConnectionSession s;

   public CachedConnectionSessionUnitTestCase (String name)
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
      sh = (CachedConnectionSessionHome)getInitialContext().lookup("CachedConnectionSession");
      s = sh.create();
      s.createTable();
   }

   protected void tearDown() throws Exception
   {
      if (s != null)
      {
         s.dropTable();
      } // end of if ()

      setSpecCompliant(Boolean.FALSE);
   }

   public static Test suite() throws Exception
   {
      Test t1 = getDeploySetup(CachedConnectionSessionUnitTestCase.class, "jcatest.jar");
      Test t2 = getDeploySetup(t1, "testadapter-ds.xml");
      return getDeploySetup(t2, "jbosstestadapter.rar");
   }

   public void testCachedConnectionSession() throws Exception
   {
      s.insert(1L, "testing");
      assertTrue("did not get expected value back", "testing".equals(s.fetch(1L)));
   }

   public void testTLDB() throws Exception
   {
      setSpecCompliant(Boolean.FALSE);
      s.firstTLTest();
   }

}// CachedConnectionSessionUnitTestCase
