/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.invokers.test;

import java.io.IOException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

import org.jboss.test.invokers.interfaces.SimpleBMP;
import org.jboss.test.invokers.interfaces.SimpleBMPHome;

import org.jboss.test.invokers.interfaces.StatelessSession;
import org.jboss.test.invokers.interfaces.StatelessSessionHome;

/**
 * Test use of multiple invokers per container
 *
 * @author    bill@burkecentral.com
 * @version   $Revision: 1.1 $
 */
public class MultiInvokersUnitTestCase extends JBossTestCase
{
   /**
    * Constructor for the CustomSocketsUnitTestCase object
    *
    * @param name  Description of Parameter
    */
   public MultiInvokersUnitTestCase(String name)
   {
      super(name);
   }


   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testMultiInvokers() throws Exception
   {
      InitialContext ctx = new InitialContext();
      SimpleBMPHome home1 = (SimpleBMPHome)ctx.lookup("SimpleBMP");
      SimpleBMPHome home2 = (SimpleBMPHome)ctx.lookup("CompressionSimpleBMP");

      SimpleBMP bmp1 = home1.create(1, "bill");
      SimpleBMP bmp2 = home2.findByPrimaryKey(new Integer(1)); // should find it.

      getLog().debug("");
      getLog().debug("bmp1 name: " + bmp1.getName());
      getLog().debug("bmp2 name: " + bmp2.getName());
      getLog().debug("setting name to burke");
      bmp1.setName("burke");
      getLog().debug("bmp1 name: " + bmp1.getName());
      getLog().debug("bmp2 name: " + bmp2.getName());
      assertTrue("bmp1 " + bmp1.getName() + "  == bmp2 " + bmp2.getName(), bmp1.getName().equals(bmp2.getName()));

      StatelessSessionHome shome1 = (StatelessSessionHome)ctx.lookup("StatelessSession");
      StatelessSessionHome shome2 = (StatelessSessionHome)ctx.lookup("CompressionStatelessSession");
      StatelessSession ss1 = shome1.create();
      StatelessSession ss2 = shome2.create();

      ss1.getBMP(1);
      ss2.getBMP(1);
      
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(MultiInvokersUnitTestCase.class, "invokers.jar");
   }

}
