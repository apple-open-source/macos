/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;

import javax.naming.InitialContext;

import org.jboss.test.cts.jms.ContainerMBox;
import org.jboss.test.cts.interfaces.CtsCmp2Session;
import org.jboss.test.cts.interfaces.CtsCmp2SessionHome;

import junit.framework.Test;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

/** Tests of versioned deployments using ear scoped class loader.
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.2.2.2 $
 */
public class CtsCmp2UnitTestCase extends JBossTestCase
{
   public CtsCmp2UnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
   }

   public void testV1() throws Exception
   {
      try 
      {
         deploy("cts-v1cmp.ear");
         InitialContext ctx = new InitialContext();
         CtsCmp2SessionHome home = (CtsCmp2SessionHome) ctx.lookup("v1/CtsCmp2SessionBean");
         CtsCmp2Session session = home.create();
         session.testV1();
      }
      finally
      {
         undeploy("cts-v1cmp.ear");
      } // end of try-catch
   }

   public void testV2() throws Exception
   {
      try 
      {
         deploy("cts-v1cmp.ear");
         try 
         {
            deploy("cts-v2cmp.ear");
            InitialContext ctx = new InitialContext();
            CtsCmp2SessionHome home = (CtsCmp2SessionHome) ctx.lookup("v2/CtsCmp2SessionBean");
            CtsCmp2Session session = home.create();
            session.testV2();
         }
         finally
         {
            undeploy("cts-v2cmp.ear");
         } // end of finally
      }
      finally
      {
         undeploy("cts-v1cmp.ear");
      } // end of try-catch
   }

   public void testV1Sar() throws Exception
   {
      try 
      {
         deploy("cts-v1cmp-sar.ear");
         InitialContext ctx = new InitialContext();
         CtsCmp2SessionHome home = (CtsCmp2SessionHome) ctx.lookup("v1/CtsCmp2SessionBean");
         CtsCmp2Session session = home.create();
         session.testV1();
      }
      finally
      {
         undeploy("cts-v1cmp-sar.ear");
      } // end of try-catch
   }

   public void testV2Sar() throws Exception
   {
      try 
      {
         getLog().debug("Deploying cts-v1cmp-sar.ear");
         deploy("cts-v1cmp-sar.ear");
         getLog().debug("Deployed cts-v1cmp-sar.ear");
         try 
         {
            getLog().debug("Deploying cts-v2cmp-sar.ear");
            deploy("cts-v2cmp-sar.ear");
            getLog().debug("Deployed cts-v2cmp-sar.ear");
            InitialContext ctx = new InitialContext();
            CtsCmp2SessionHome home = (CtsCmp2SessionHome) ctx.lookup("v2/CtsCmp2SessionBean");
            getLog().debug("Found CtsCmp2SessionHome");
            CtsCmp2Session session = home.create();
            getLog().debug("Created CtsCmp2Session");
            session.testV2();
            getLog().debug("Invoked CtsCmp2Session.testV2()");
         }
         finally
         {
            undeploy("cts-v2cmp-sar.ear");
         } // end of finally
      }
      finally
      {
         undeploy("cts-v1cmp-sar.ear");
      } // end of try-catch
   }

   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new CtsCmp2UnitTestCase("testV1"));
      suite.addTest(new CtsCmp2UnitTestCase("testV2"));
      suite.addTest(new CtsCmp2UnitTestCase("testV1Sar"));
      suite.addTest(new CtsCmp2UnitTestCase("testV2Sar"));
      return suite;
   }
}
