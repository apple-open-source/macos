/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when an invalid expression is passed to a query construction
 * method.
 *
 * @see javax.management.ValueExp
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class BadBinaryOpValueExpException
   extends Exception
{
   // Attributes ----------------------------------------------------

   /**
    * The invalid expression.
    */
   private ValueExp exp = null;
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new BadBinaryValueExpException with the given expression.
    *
    * @param exp the invalid expression
    */
   public BadBinaryOpValueExpException(ValueExp exp)
   {
      super();
      this.exp = exp;
   }

   // Public --------------------------------------------------------

   /**
    * Retrieve the bad binary value expression.
    *
    * @return the expression.
    */
   public ValueExp getExp()
   {
      return exp;
   }

   // Exception Overrides -------------------------------------------

   /**
    * Returns a string representing the error.
    *
    * @return the error string.
    */
    public String toString()
    {
       return "Bad binary operation value expression: " + exp.toString();
    }
}
