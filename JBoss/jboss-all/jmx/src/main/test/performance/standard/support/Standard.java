/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.performance.standard.support;

import javax.management.Attribute;

/**
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 *   
 */
public class Standard
   implements StandardMBean
{

   private int counter = 0;
   

   public void bogus1() {}
   public void bogus2() {}
   public void bogus3() {}
   public void bogus4() {}
   public void bogus5() {}


   public void methodInvocation() {}
   
   public void counter() {
     //++counter;
   }
   
   public int getCount() {
      return counter;
   }
      
   Integer int1;
   int int2;
   Object[][][] space;
   Attribute attr;
   
   public void mixedArguments(Integer int1, int int2, Object[][][] space, Attribute attr) {
     ++counter;
     
     this.int1 = int1;
     this.int2 = int2;
     this.space = space;
     this.attr = attr;
   }
   
   public void bogus6() {}
   public void bogus7() {}
   public void bogus8() {}
   public void bogus9() {}
   public void bogus10() {}
   
}
      



