/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.performance.standard.support;

import javax.management.Attribute;

/**
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class Standard
   implements StandardMBean
{

   private int counter = 0;
   
   public void methodInvocation() {}
   
   public void counter() {
     ++counter;
   }
   
   public int getCount() {
      return counter;
   }
      
   public void mixedArguments(Integer int1, int int2, Object[][][] space, Attribute attr) {
   
   }
   
}
      



