/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.hello.test;

import javax.ejb.*;
import javax.naming.*;

import org.jboss.test.hello.interfaces.*;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;


/** 
 *   @author Adrian Brock Adrian@jboss.org
 *   @version $Revision: 1.1.2.1 $
 */
public class HelloCachedUnitTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   public HelloCachedUnitTestCase(String name)
   {
      super(name);
   }
   
   // Public --------------------------------------------------------
   
   /**
    *  Test we get the bean interface.
    *
    * @exception   Exception
    */
   public void testHello()
      throws Exception
   {
      HelloHome home = (HelloHome)getInitialContext().lookup("helloworld/HelloCached");
      Hello hello = home.create();
      assertTrue(hello == home.create());
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(HelloCachedUnitTestCase.class, "hello.jar");
   }
}
