/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.hello.test;

import javax.naming.Context;
import javax.naming.InitialContext;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.hello.interfaces.Hello;
import org.jboss.test.hello.interfaces.HelloData;
import org.jboss.test.hello.interfaces.HelloHome;
import org.jboss.test.hello.interfaces.HelloLog;
import org.jboss.test.hello.interfaces.HelloLogHome;

/** Simple tests of the Hello stateless session bean
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.1.4.4 $
 */
public class HelloHttpStressTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   static final String JNDI_NAME = "helloworld/HelloHTTP";
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------
   static boolean deployed = false;

   // Constructors --------------------------------------------------
   public HelloHttpStressTestCase(String name)
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
      InitialContext ctx = getInitialContext();
      HelloHome home = (HelloHome) ctx.lookup("helloworld/HelloHTTP");
      Hello hello = home.create();
      String reply = hello.hello("World");
      getLog().debug(reply);
      hello.remove();
   }

   /** Execute the loggedHello call to have the session create an entity and
    * then find the entity and query the loggedHello times.
    *
    * @throws Exception
    */
   public void testLoggedHello()
      throws Exception
   {
      InitialContext ctx = getInitialContext();
      HelloHome home = (HelloHome) ctx.lookup("helloworld/HelloHTTP");
      Hello hello = home.create();
      String reply = hello.loggedHello("World");
      getLog().debug(reply);
      hello.remove();

      // Find
      HelloLogHome logHome = (HelloLogHome) ctx.lookup("helloworld/HelloLogHTTP");
      HelloLog log = logHome.findByPrimaryKey("World");
      long start = log.getStartTime();
      long end = log.getEndTime();
      getLog().debug("HelloLog times: "+start+","+end);
      long elapsed = log.getElapsedTime();
      getLog().debug("HelloLog elapsed: "+elapsed);
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
      name.setName("World");
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
         hello.hello("Rickard");
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
         HelloHome home = (HelloHome) new InitialContext().lookup(JNDI_NAME);
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
         HelloHome home = (HelloHome) ctx.lookup(JNDI_NAME);
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
   }


   public static Test suite() throws Exception
   {
      return getDeploySetup(HelloHttpStressTestCase.class, "hello.jar");
   }

}
