/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.lock.test;

import java.io.IOException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.apache.log4j.Category;
import org.jboss.test.JBossTestCase;

import org.jboss.test.lock.interfaces.EnterpriseEntity;
import org.jboss.test.lock.interfaces.EnterpriseEntityHome;

/**
 * Test of EJB call invocation overhead.
 *
 * @author    Scott.Stark@jboss.org
 * @version   $Revision: 1.4 $
 */
public class SpinUnitTestCase extends JBossTestCase
{
   /**
    * Constructor for the SpinUnitTestCase object
    *
    * @param name  Description of Parameter
    */
   public SpinUnitTestCase(String name)
   {
      super(name);
   }

   static void create() throws Exception
   {
      InitialContext jndiContext = new InitialContext();
      Object obj = jndiContext.lookup("EnterpriseEntity_A");
      obj = PortableRemoteObject.narrow(obj, EnterpriseEntityHome.class);
      EnterpriseEntityHome home = (EnterpriseEntityHome)obj;
      try
      {
         home.create("Bean1");
      }
      catch (CreateException e)
      {
      }
   }

   static void remove() throws Exception
   {
      InitialContext jndiContext = new InitialContext();
      Object obj = jndiContext.lookup("EnterpriseEntity_A");
      obj = PortableRemoteObject.narrow(obj, EnterpriseEntityHome.class);
      EnterpriseEntityHome home = (EnterpriseEntityHome)obj;
      try
      {
         home.remove("Bean1");
      }
      catch (RemoveException e)
      {
      }
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testContention() throws Exception
   {
      getLog().debug("+++ testContention()");
      InitialContext jndiContext = new InitialContext();
      Object obj = jndiContext.lookup("EnterpriseEntity_A");
      obj = PortableRemoteObject.narrow(obj, EnterpriseEntityHome.class);
      EnterpriseEntityHome home = (EnterpriseEntityHome)obj;
      getLog().debug("Found EnterpriseEntityHome @ jndiName=EnterpriseEntity");
      Run r0 = new Run(home.findByPrimaryKey("Bean1"), getLog());
      Run r1 = new Run(home.findByPrimaryKey("Bean1"), getLog());
      Run r2 = new Run(home.findByPrimaryKey("Bean1"), getLog());
      Thread t0 = new Thread(r0);
      Thread t1 = new Thread(r1);
      Thread t2 = new Thread(r2);
      t0.start();
      Thread.sleep(100);
      t1.start();
      Thread.sleep(100);
      t2.start();
      getLog().debug("Waiting for t0...");
      try
      {
         t0.join(5000);
         assertTrue(r0.ex == null);
      }
      catch (InterruptedException e)
      {
         getLog().debug("Timed out waiting for t1");
      }
      getLog().debug("Waiting for t1...");
      try
      {
         t1.join(5000);
         assertTrue(r1.ex == null);
      }
      catch (InterruptedException e)
      {
         getLog().debug("Timed out waiting for t1");
      }
      getLog().debug("Waiting for t2...");
      try
      {
         t2.join(5000);
         assertTrue(r2.ex == null);
      }
      catch (InterruptedException e)
      {
         getLog().debug("Timed out waiting for t2");
      }

      getLog().debug("End threads");
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp() throws Exception
   {
      try
      {
         create();
      }
      catch (Exception e)
      {
         getLog().error("setup error in create: ", e);
         throw e;
      }
   }

   /**
    * The teardown method for JUnit
    *
    * @exception Exception  Description of Exception
    */
   protected void tearDown() throws Exception
   {
      try
      {
         remove();
      }
      catch (Exception e)
      {
         getLog().error("teardown error in remove: ", e);
         throw e;
      }
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(SpinUnitTestCase.class, "locktest.jar");
   }



   /**
    * #Description of the Class
    */
   static class Run implements Runnable
   {
      EnterpriseEntity bean;
      Exception ex;
      private Category log;

      Run(EnterpriseEntity bean, Category log)
      {
         this.bean = bean;
         this.log = log;
      }

      /**
       * Main processing method for the Run object
       */
      public synchronized void run()
      {
         notifyAll();
         try
         {
            long start = System.currentTimeMillis();
            bean.sleep(5000);
            long end = System.currentTimeMillis();
            long elapsed = end - start;
            log.debug(" bean.sleep() time = " + elapsed + " ms");
         }
         catch (Exception e)
         {
            ex = e;
         }
      }
   }

}
