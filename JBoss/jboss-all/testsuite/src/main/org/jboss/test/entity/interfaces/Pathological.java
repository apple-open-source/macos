/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.entity.interfaces;

/**
 * A trigger for pathological behaviour.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class Pathological
{
   private static ThreadLocal pathological = new ThreadLocal();

   public static boolean isPathological()
   {
      Boolean result = (Boolean) pathological.get();
      if (result != null)
         return result.booleanValue();
      else
         return false;
   }

   public static void setPathological(boolean value)
   {
      pathological.set(new Boolean(value));
   }
}
