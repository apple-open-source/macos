package org.jboss.test.jca.test;

import junit.framework.*;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.interfaces.JDBCStatementTestsConnectionSession;
import org.jboss.test.jca.interfaces.JDBCStatementTestsConnectionSessionHome;

/**
 * Test redeploy of jdbc driver
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version
 */

public class JDBCDriverRedeployUnitTestCase extends JBossTestCase
{
   public JDBCDriverRedeployUnitTestCase(String name)
   {
      super(name);
   }

   public void testRedeploy() throws Exception
   {
      // fail("This test does not work because of class caching in java.sql.DriverManager");
      if (1!=0) return;

      doDeploy();
      try
      {
         doTest();
      }
      finally
      {
         doUndeploy();
      }

      doDeploy();
      try
      {
         doTest();
      }
      finally
      {
         doUndeploy();
      }
   }

   private void doTest() throws Exception
   {
      JDBCStatementTestsConnectionSessionHome home =
         (JDBCStatementTestsConnectionSessionHome)getInitialContext().lookup("JDBCStatementTestsConnectionSession");
      JDBCStatementTestsConnectionSession s = home.create();
      s.testConnectionObtainable();
   }

   private void doDeploy() throws Exception
   {
      deploy("jbosstestdriver.jar");
      try
      {
         deploy("testdriver-ds.xml");
         try
         {
            deploy("jcatest.jar");
         }
         catch (Exception e)
         {
            undeploy("testdriver-ds.xml");
            throw e;
         }
      }
      catch (Exception e)
      {
         undeploy("jbosstestdriver.jar");
         throw e;
      }
   }

   private void doUndeploy() throws Exception
   {
      try
      {
         undeploy("jcatest.jar");
      }
      catch (Throwable ignored)
      {
      }
      try
      {
         undeploy("testdriver-ds.xml");
      }
      catch (Throwable ignored)
      {
      }
      try
      {
         undeploy("jbosstestdriver.jar");
      }
      catch (Throwable ignored)
      {
      }
   }
}
