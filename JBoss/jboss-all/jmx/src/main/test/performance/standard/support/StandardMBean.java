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
public interface StandardMBean
{
   void bogus1();
   void bogus2();
   void bogus3();
   void bogus4();
   void bogus5();
   
   void methodInvocation();
   void counter();
   void mixedArguments(Integer int1, int int2, Object[][][] space, Attribute attr);
   
   void bogus6();
   void bogus7();
   void bogus8();
   void bogus9();
   void bogus10();
   
   
}
      



