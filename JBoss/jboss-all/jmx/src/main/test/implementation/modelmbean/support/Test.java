/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.modelmbean.support;

/**
 * Trivial POJO to test MMB attribute caching.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class Test
{
   // Attributes ----------------------------------------------------
   int fooCount = 0;
   int barCount = 0;
   String value = null;
   
   // Management interface implementation ---------------------------
   public void setFoo(String value)
   {
      fooCount++;
      this.value = value;
   }

   public String getBar()
   {
      barCount++;
      return value;
   }
   
   // Public --------------------------------------------------------
   public int getFooCount()
   {
      return fooCount;
   }
   
   public int getBarCount()
   {
      return barCount;
   }
}
      



