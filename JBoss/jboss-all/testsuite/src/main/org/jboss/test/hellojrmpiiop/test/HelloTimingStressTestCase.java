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
package org.jboss.test.hellojrmpiiop.test;


import javax.ejb.*;
import javax.naming.*;
import javax.rmi.PortableRemoteObject;

import org.jboss.test.hellojrmpiiop.interfaces.*;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;


/** Simple tests of the Hello stateless session bean
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.2 $
 */
public class HelloTimingStressTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   private java.util.Properties cosnamingJndiProps;
   private java.util.Properties iiopJndiProps;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   public HelloTimingStressTestCase(String name) 
       throws java.io.IOException
   {
      super(name);
      java.net.URL url;

      url = ClassLoader.getSystemResource("iiop.jndi.properties");
      iiopJndiProps = new java.util.Properties();
      iiopJndiProps.load(url.openStream());

      url = ClassLoader.getSystemResource("cosnaming.jndi.properties");
      cosnamingJndiProps = new java.util.Properties();
      cosnamingJndiProps.load(url.openStream());
   }
   
   // Package --------------------------------------------------------

   InitialContext getInitialContext(java.util.Properties jndiProps) 
       throws Exception
   {
      return new InitialContext(jndiProps);
   }

   // The two methods below are protected in the superclass. Here they're 
   // redefined as public to be called by the anonymous local Thread subclasses
   // within testConcurrentJrmpAndIiopInvocations().

   public InitialContext getInitialContext() 
       throws Exception
   {
      return super.getInitialContext();
   }

   public int getIterationCount()
   {
      return super.getIterationCount();
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
      HelloHome home;
      Hello hello; 

      // use JRMP invoker 
      // (home is a JRMP proxy gotten from the JNP naming service)
      home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext().lookup(HelloHome.JNDI_NAME),
                               HelloHome.class);
      hello = home.create();
      getLog().debug(hello.hello("World"));
      hello.remove();

      // use IIOP invoker 
      // (home is an IOR gotten from the JNP naming service)
      home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext(iiopJndiProps).lookup(
                                                          HelloHome.JNDI_NAME),
                               HelloHome.class);
      hello = home.create();
      getLog().debug(hello.hello("World"));
      hello.remove();

      // use IIOP invoker 
      // (home is an IOR gotten from the CORBA naming service)
      home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext(cosnamingJndiProps).lookup(
                                                          HelloHome.JNDI_NAME),
                               HelloHome.class);
      hello = home.create();
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
      HelloHome home;
      Hello hello; 
      HelloData name;

      // use JRMP invoker 
      // (home is a JRMP proxy gotten from the JNP naming service)
      home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext().lookup(HelloHome.JNDI_NAME),
                               HelloHome.class);
      hello = home.create();
      name = new HelloData();
      name.setName("World");
      getLog().debug(hello.howdy(name));
      hello.remove();

      // use IIOP invoker 
      // (home is an IOR gotten from the JNP naming service)
      home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext(iiopJndiProps).lookup(
                                                          HelloHome.JNDI_NAME),
                               HelloHome.class);
      hello = home.create();
      name = new HelloData();
      name.setName("World");
      getLog().debug(hello.howdy(name));
      hello.remove();

      // use IIOP invoker 
      // (home is an IOR gotten from the CORBA naming service)
      home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext(cosnamingJndiProps).lookup(
                                                          HelloHome.JNDI_NAME),
                               HelloHome.class);
      hello = home.create();
      name = new HelloData();
      name.setName("World");
      getLog().debug(hello.howdy(name));
      hello.remove();

   }
   
   /**
    *   This tests the speed of JRMP invocations
    *
    * @exception   Exception
    */
   public void testJRMPSpeed()
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
    *   This tests the speed of IIOP invocations
    *
    * @exception   Exception
    */
   public void testIIOPSpeed()
      throws Exception
   {
      long start = System.currentTimeMillis();
      HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext(iiopJndiProps).lookup(
                                                          HelloHome.JNDI_NAME),
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
    *   This tests the speed of JRMP invocations
    *
    * @exception   Exception
    */
   public void testJRMPSpeed2()
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
    *   This tests the speed of IIOP invocations
    *
    * @exception   Exception
    */
   public void testIIOPSpeed2()
      throws Exception
   {
      long start = System.currentTimeMillis();
      long start2 = start;
      HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                               getInitialContext(iiopJndiProps).lookup(
                                                          HelloHome.JNDI_NAME),
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
    *   This tests concurrent JRMP and IIOP invocations
    *
    * @exception   Exception
    */
   public void testConcurrentJrmpAndIiopInvocations()
      throws Exception
   {
      Thread t1 = new Thread() {
         public void run() {
            try 
            {
               HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                     getInitialContext().lookup(HelloHome.JNDI_NAME),
                     HelloHome.class);
               Hello hello = home.create();
               for (int i = 0 ; i < getIterationCount(); i++)
               {
                  sleep(100);
                  hello.hello("Rickard");
               }
               for (int i = 0 ; i < getIterationCount(); i++)
               {
                  sleep(100);
                  hello.helloHello(hello);
               }
            }
            catch (Exception e) 
            {
               e.printStackTrace();
            }
         }
      };

      Thread t2 = new Thread() {
         public void run() {
            try 
            {
               HelloHome home = (HelloHome)PortableRemoteObject.narrow(
                     getInitialContext(cosnamingJndiProps).lookup(
                                                          HelloHome.JNDI_NAME),
                     HelloHome.class);
               Hello hello = home.create();
               for (int i = 0 ; i < getIterationCount() ; i++)
               {
                  sleep(10);
                  hello.hello("Rickard");
               }
               for (int i = 0 ; i < getIterationCount() ; i++)
               {
                  sleep(10);
                  hello.helloHello(hello);
               }
            }
            catch (Exception e) 
            {
               e.printStackTrace();
            }
         }
      };
      
      t1.start();
      t2.start();

      t1.join();
      t2.join();
      
   }
   
   public static Test suite() throws Exception
   {
      return getDeploySetup(HelloTimingStressTestCase.class, "hellojrmpiiop.jar");
   }

}
