/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.query.support;

public class NumberTest implements NumberTestMBean
{
   private Number number;

   public NumberTest()
   {
   }

   public NumberTest(double number)
   {
      this.number = new Double(number);
   }

   public NumberTest(Double number)
   {
      this.number = number;
   }

   public NumberTest(float number)
   {
      this.number = new Float(number);
   }

   public NumberTest(Float number)
   {
      this.number = number;
   }

   public NumberTest(int number)
   {
      this.number = new Integer(number);
   }

   public NumberTest(Integer number)
   {
      this.number = number;
   }

   public NumberTest(long number)
   {
      this.number = new Long(number);
   }

   public NumberTest(Long number)
   {
      this.number = number;
   }

   public Number getNumber()
   {
      return number;
   }
}
