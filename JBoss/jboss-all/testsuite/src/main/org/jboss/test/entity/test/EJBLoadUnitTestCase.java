/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entity.test;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;

import org.jboss.test.entity.interfaces.EJBLoad;
import org.jboss.test.entity.interfaces.EJBLoadHome;

/**
 * Test that ejbLoad is called.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.4.1 $
 */
public class EJBLoadUnitTestCase
   extends JBossTestCase
{
   public EJBLoadUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(EJBLoadUnitTestCase.class, "jboss-test-ejbload.jar");
   }

   public void testNoTransactionCommitB()
      throws Exception
   {
      getLog().debug("Retrieving enitity");
      EJBLoad entity = getEJBLoadHomeB().findByPrimaryKey("Entity");
      entity.wasEJBLoadCalled();

      getLog().debug("Testing that ejb load is invoked again");
      entity.noTransaction();
      assertTrue("Should reload for option b after access outside a transaction", entity.wasEJBLoadCalled());
   }

   public void testNoTransactionCommitC()
      throws Exception
   {
      getLog().debug("Retrieving enitity");
      EJBLoad entity = getEJBLoadHomeC().findByPrimaryKey("Entity");
      entity.wasEJBLoadCalled();

      getLog().debug("Testing that ejb load is invoked again");
      entity.noTransaction();
      assertTrue("Should reload for option c after access outside a transaction", entity.wasEJBLoadCalled());
   }

   private EJBLoadHome getEJBLoadHomeB()
      throws Exception
   {
      return (EJBLoadHome) getInitialContext().lookup("EJBLoadB");
   }

   private EJBLoadHome getEJBLoadHomeC()
      throws Exception
   {
      return (EJBLoadHome) getInitialContext().lookup("EJBLoadC");
   }
}
