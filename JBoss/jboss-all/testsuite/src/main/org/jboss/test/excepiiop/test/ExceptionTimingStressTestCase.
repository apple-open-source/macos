/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.excepiiop.test;


import javax.ejb.*;
import javax.naming.*;
import javax.rmi.PortableRemoteObject;

import org.jboss.test.excepiiop.interfaces.*;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;


public class ExceptionTimingStressTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   private java.util.Properties jndiProps;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   public ExceptionTimingStressTestCase(String name) 
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
   
   public void testNoException()
      throws Exception
   {
      ExceptionThrowerHome home = 
         (ExceptionThrowerHome)PortableRemoteObject.narrow(
               getInitialContext().lookup(ExceptionThrowerHome.JNDI_NAME),
               ExceptionThrowerHome.class);
      ExceptionThrower exceptionThrower = home.create();
      exceptionThrower.throwException(0);
      exceptionThrower.remove();
   }
   
   public void testJavaException()
      throws Exception
   {
      ExceptionThrowerHome home = 
         (ExceptionThrowerHome)PortableRemoteObject.narrow(
               getInitialContext().lookup(ExceptionThrowerHome.JNDI_NAME),
               ExceptionThrowerHome.class);
      ExceptionThrower exceptionThrower = home.create();
      try 
      {
         exceptionThrower.throwException(1);
      }
      catch (JavaException e)
      {
         System.out.println("JavaException: " + e.i + ", " + e.s);
      }
      exceptionThrower.remove();
   }
   
   public void testIdlException()
      throws Exception
   {
      ExceptionThrowerHome home = 
         (ExceptionThrowerHome)PortableRemoteObject.narrow(
               getInitialContext().lookup(ExceptionThrowerHome.JNDI_NAME),
               ExceptionThrowerHome.class);
      ExceptionThrower exceptionThrower = home.create();
      try 
      {
         exceptionThrower.throwException(-1);
      }
      catch (IdlException e)
      {
         System.out.println("IdlException: " + e.i + ", " + e.s);
      }
      exceptionThrower.remove();
   }
   
   /**
    *   This tests the speed of invocations
    *
    * @exception   Exception
    */
   public void testSpeedNoException()
      throws Exception
   {
      long start = System.currentTimeMillis();
      ExceptionThrowerHome home = 
         (ExceptionThrowerHome)PortableRemoteObject.narrow(
               getInitialContext().lookup(ExceptionThrowerHome.JNDI_NAME),
               ExceptionThrowerHome.class);
      ExceptionThrower exceptionThrower = home.create();
      for (int i = 0 ; i < getIterationCount(); i++)
      {
         exceptionThrower.throwException(0);
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
      exceptionThrower.remove();
   }
   
   /**
    *   This tests the speed of invocations
    *
    * @exception   Exception
    */
   public void testSpeedJavaException()
      throws Exception
   {
      long start = System.currentTimeMillis();
      ExceptionThrowerHome home = 
         (ExceptionThrowerHome)PortableRemoteObject.narrow(
               getInitialContext().lookup(ExceptionThrowerHome.JNDI_NAME),
               ExceptionThrowerHome.class);
      ExceptionThrower exceptionThrower = home.create();
      for (int i = 0 ; i < getIterationCount(); i++)
      {
         try 
         {
            exceptionThrower.throwException(i + 1);
         }
         catch (JavaException e)
         {
            System.out.println("JavaException: " + e.i + ", " + e.s);
         }
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
      exceptionThrower.remove();
   }
   
   /**
    *   This tests the speed of invocations
    *
    * @exception   Exception
    */
   public void testSpeedIdlException()
      throws Exception
   {
      long start = System.currentTimeMillis();
      ExceptionThrowerHome home = 
         (ExceptionThrowerHome)PortableRemoteObject.narrow(
               getInitialContext().lookup(ExceptionThrowerHome.JNDI_NAME),
               ExceptionThrowerHome.class);
      ExceptionThrower exceptionThrower = home.create();
      for (int i = 0 ; i < getIterationCount(); i++)
      {
         try 
         {
            exceptionThrower.throwException(-1 - i);
         }
         catch (IdlException e)
         {
            System.out.println("IdlException: " + e.i + ", " + e.s);
         }
      }
      long end = System.currentTimeMillis();
      getLog().debug("Avg. time/call(ms):"+((end-start)/getIterationCount()));
      exceptionThrower.remove();
   }
   
   public static Test suite() throws Exception
   {
      return getDeploySetup(ExceptionTimingStressTestCase.class, "excepiiop.jar");
   }

}
