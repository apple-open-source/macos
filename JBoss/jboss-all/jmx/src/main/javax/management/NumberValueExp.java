/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A Number that is an arguement to a query.<p>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3 $
 */
/*package*/ class NumberValueExp
   extends SingleValueExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   // Static  -----------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a number value expression for the passed number
    *
    * @param value the value of number
    */
   public NumberValueExp(Number value)
   {
      super(value);
   }

   // Public ------------------------------------------------------

   /**
    * Test whether the type is integer
    */
   public boolean isInteger()
   {
       Object value = getValue();
       return value instanceof Integer || value instanceof Long;
   }

   /**
    * Get the value of this number (integers)
    */
   public double getLongValue()
   {
       return ((Number)getValue()).longValue();
   }

   /**
    * Get the value of this number (floating)
    */
   public double getDoubleValue()
   {
       return ((Number)getValue()).doubleValue();
   }

   // X Implementation --------------------------------------------

   // Y overrides -------------------------------------------------

   // Protected ---------------------------------------------------

   // Package Private ---------------------------------------------

   // Private -----------------------------------------------------

   // Inner Classes -----------------------------------------------
}
