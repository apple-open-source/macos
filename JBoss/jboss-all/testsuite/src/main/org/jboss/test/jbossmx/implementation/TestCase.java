/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.implementation;

import org.jboss.test.JBossTestCase;

public class TestCase
   extends JBossTestCase
{
   public TestCase(String s)
   {
      super(s);
   }

   /**
    * We do not need the JBoss Server for implementation tests
    */
   public void testServerFound()
   {
   }
}
