/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.query.support;

public class Trivial implements TrivialMBean
{
   private String string = "trivial";
   private int number = 0;

   public Trivial()
   {
   }

   public Trivial(int number)
   {
      this.number = number;
   }

   public String getString()
   {
      return string;
   }

   public Integer getNumber()
   {
      return new Integer(number);
   }
}
