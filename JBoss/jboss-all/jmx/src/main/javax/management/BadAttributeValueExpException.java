/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when an invalid attribute value is passed to a query construction
 * method.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class BadAttributeValueExpException
   extends Exception
{
   // Attributes ----------------------------------------------------

   /**
    * The invalid value.
    */
   private Object val = null;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new BadAttributeValueExpException with the given value.
    *
    * @param val the invalid value
    */
   public BadAttributeValueExpException(Object val)
   {
      super();
      this.val = val;
   }

   // Public --------------------------------------------------------

   // Exception Overrides -------------------------------------------

   /**
    * Returns a string representing the error.
    *
    * @return the error string.
    */
   public String toString()
   {
      return "Bad attribute value expression: " + val.toString();
   }

   // Private -------------------------------------------------------
}

