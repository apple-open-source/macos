/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.helloiiop.test;


import javax.ejb.*;
import javax.naming.*;
import javax.rmi.PortableRemoteObject;

import org.jboss.test.helloiiop.interfaces.*;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;


/** Simple tests of the Hello stateless session bean
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.3 $
 */
public class HelloTimingStressTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   private java.util.Properties jndiProps;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   public HelloTimingStressTestCase(String name) 
       throws java.io.IOException
   {
      super(name);
      java.net.URL url 
          = ClassLoader.getSystemResource("cosnaming.jndi.properties");
      jndiProps = new java.util.Properties();
      jndiProps.load(url.openStream());
   }
   
   // Override getInitialContext() ----------------------------------
   protected InitialContext getInitialContext() throws Exception
   {
      return new InitialContext(jndiProps);
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
      HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext().lookup(HelloHome.JNDI_NAME),
                               HelloHome.class);
      Hello hello = home.create();
      getLog().debug(hello.hello("World"));
      hello.remove();
   }
   
   /**
    *   Test marshalling of custom data-holders.
    *
    * @exception   Exception
    */
   public void testData()
      throws Exception
   {
      HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext().lookup(HelloHome.JNDI_NAME),
                               HelloHome.class);
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
      HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext().lookup(HelloHome.JNDI_NAME),
                               HelloHome.class);
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
      long start2 = start;
      HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext().lookup(HelloHome.JNDI_NAME),
                               HelloHome.class);
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
         HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                              getInitialContext().lookup(HelloHome.JNDI_NAME),
                              HelloHome.class);
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
         HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                                  ctx.lookup(HelloHome.JNDI_NAME),
                                  HelloHome.class);
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
   }
   
   public static Test suite() throws Exception
   {
      return getDeploySetup(HelloTimingStressTestCase.class, "helloiiop.jar");
   }

}
