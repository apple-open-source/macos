/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.hello.test;

import java.io.File;
import javax.naming.Context;
import javax.naming.InitialContext;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.hello.interfaces.Hello;
import org.jboss.test.hello.interfaces.HelloData;
import org.jboss.test.hello.interfaces.HelloException;
import org.jboss.test.hello.interfaces.HelloHome;
import org.jboss.test.hello.interfaces.NotSerializable;

/** Simple tests of the Hello stateless session bean
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.1.4.2 $
 */
public class HelloClusteredHttpStressTestCase extends JBossTestCase
{
   static final String JNDI_NAME = "helloworld/HelloHA-HTTP";

   // Constructors --------------------------------------------------
   public HelloClusteredHttpStressTestCase(String name)
   {
      super(name);
   }
   
   // Public --------------------------------------------------------
   
   /**
    *   Lookup the bean, call it, remove it.
    *
    * @exception   Exception
    */
   public void testHello()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      getLog().debug(hello.hello("testHello"));
      hello.remove();
   }

   /**
    *   Lookup the bean, call it, remove it.
    *
    * @exception   Exception
    */
   public void testSleepingHello()
      throws Exception
   {
      HelloHome home = (HelloHome) getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      getLog().debug(hello.sleepingHello("testSleepingHello", 10000));
      hello.remove();
   }

   /** Test that an application declared exception is not wrapped and does
    * not trigger failover.
    * @throws Exception
    */
   public void testHelloException()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      try
      {
         getLog().debug("Invoking helloException");
         hello.helloException("testHelloException");
         fail("Was able to call helloException");
      }
      catch(HelloException e)
      {
         getLog().debug("Caught HelloException as expected");
      }
      hello.remove();
   }

   /** Test that a runtime error does trigger failover.
    * @throws Exception
    */
   public void testCNFEObject()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      try
      {
         // Remove
         File clazz = new File("classes/org/jboss/test/hello/ejb/HelloBean$ServerData.class");
         clazz.delete();
         getLog().debug("Invoking getCNFEObject");
         hello.getCNFEObject();
      }
      catch(Exception e)
      {
         getLog().debug("Caught ClassNotFoundException as expected", e);
      }
   }

   public void testServerExceptionDoesntFailOver()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      try
      {
         hello.throwException();
      }
      catch(Exception e)
      {
         getLog().debug("Caught IOException as expected", e);
      }
      hello.hello("server exception error");
   }

   public void testClientSerializationErrorDoesntFailOver()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      try
      {
         hello.setNotSerializable(new NotSerializable());
      }
      catch(Exception e)
      {
         getLog().debug("Caught IOException as expected", e);
      }
      hello.hello("client serialization error");
   }

   public void testServerSerializationErrorDoesntFailOver()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      try
      {
         hello.getNotSerializable();
      }
      catch(Exception e)
      {
         getLog().debug("Caught IOException as expected", e);
      }
      hello.hello("server serialization error");
   }

   /**
    *   Test marshalling of custom data-holders.
    *
    * @exception   Exception
    */
   public void testData()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      HelloData name = new HelloData();
      name.setName("testData");
      getLog().debug(hello.howdy(name));
      hello.remove();
   }
   
   /**
    *   This tests the speed of invocations
    *
    * @exception   Exception
    */
   public void testSpeed()
      throws Exception
   {
      long start = System.currentTimeMillis();
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      for (int i = 0 ; i < getIterationCount(); i++)
      {
         hello.hello("testSpeed");
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
   }
   
   /**
    *   This tests the speed of invocations
    *
    * @exception   Exception
    */
   public void testSpeed2()
      throws Exception
   {
      long start = System.currentTimeMillis();
      long start2 = start;
      HelloHome home = (HelloHome)getInitialContext().lookup(JNDI_NAME);
      Hello hello = home.create();
      for (int i = 0 ; i < getIterationCount(); i++)
      {
         hello.helloHello(hello);
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
   }

   /**
    *   This tests the speed of InitialContext lookups
    * including getting the initial context.
    * @exception   Exception
    */
   public void testContextSpeed()
      throws Exception
   {
      long start = System.currentTimeMillis();
      
      getLog().debug("Starting context lookup speed test");
      for (int i = 0; i < getIterationCount(); i++)
      {
         HelloHome home = (HelloHome)new InitialContext().lookup(JNDI_NAME);
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
   }

   /**
    *   This tests the speed of JNDI lookups
    *
    * @exception   Exception
    */
   public void testReusedContextSpeed()
      throws Exception
   {
      Context ctx = getInitialContext();
      long start = System.currentTimeMillis();
      
      getLog().debug("Starting context lookup speed test");
      for (int i = 0; i < getIterationCount(); i++)
      {
         HelloHome home = (HelloHome)ctx.lookup(JNDI_NAME);
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
   }
   
   
   public static Test suite() throws Exception
   {
      return getDeploySetup(HelloClusteredHttpStressTestCase.class, "hello.jar");
   }

}
