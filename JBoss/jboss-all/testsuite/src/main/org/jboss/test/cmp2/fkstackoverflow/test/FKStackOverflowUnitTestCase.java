/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkstackoverflow.test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.fkstackoverflow.ejb.FacadeUtil;
import junit.framework.Test;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class FKStackOverflowUnitTestCase
   extends JBossTestCase
{
   // Constructor

   public FKStackOverflowUnitTestCase(String name)
   {
      super(name);
   }

   // Suite

   public static Test suite() throws Exception
   {
      return getDeploySetup(FKStackOverflowUnitTestCase.class, "cmp2-fkstackoverflow.jar");
   }

   // Tests

   public void testSimpleScenario()
      throws Exception
   {
      FacadeUtil.getHome().create().testSimple();
   }

   public void testComplexScenario()
      throws Exception
   {
      FacadeUtil.getHome().create().testComplex();
   }
}
