/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkmapping.ejb;

import javax.ejb.EJBException;

/**
 * Instances of this exception is thrown from assertTrue(msg,expression) methods
 * if expression evaluates to false.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class AssertionException
   extends EJBException
{
   // Constructor ----------------------------------
   public AssertionException(String message)
   {
      super(message);
   }

   public AssertionException(String message, Exception cause)
   {
      super(message, cause);
   }
}
