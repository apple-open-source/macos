/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * A Between Query Expression.<p>
 *
 * Returns true only when the test expression is between the lower and
 * upper bounds inclusive.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020314 Adrian Brock:</b>
 * <ul>
 * <li>Fix the human readable expression
 * </ul>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3 $
 */
/*package*/ class BetweenQueryExp
   extends QueryExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The value to test
    */
   ValueExp test;

   /**
    * The lower bound
    */
   ValueExp lower;

   /**
    * The upper bound
    */
   ValueExp upper;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Create a new BETWEEN query Expression
    * 
    * @param test the value to test
    * @param lower the lower bound
    * @param upper the upper bound
    */
   public BetweenQueryExp(ValueExp test, ValueExp lower, ValueExp upper)
   {
      this.test = test;
      this.lower = lower;
      this.upper = upper;
   }

   // Public ------------------------------------------------------

   // QueryExp implementation -------------------------------------

   public boolean apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      ValueExp calcTest = test.apply(name);
      ValueExp calcLower = lower.apply(name);
      ValueExp calcUpper = upper.apply(name);

      // Number
      if (calcTest instanceof NumberValueExp)
      {
         // REVIEW: Exceptions for cast problems, which one?
         double valueTest = ((NumberValueExp) calcTest).getDoubleValue();
         double valueLower = ((NumberValueExp) calcLower).getDoubleValue();
         double valueUpper = ((NumberValueExp) calcUpper).getDoubleValue();
         return (valueLower <= valueTest && valueTest <= valueUpper);
      }
      // String
      else if (calcTest instanceof StringValueExp)
      {
         // REVIEW: Exceptions for cast problems, which one?
         String valueTest = ((StringValueExp) calcTest).toString();
         String valueLower = ((StringValueExp) calcLower).toString();
         String valueUpper = ((StringValueExp) calcUpper).toString();
         return (valueLower.compareTo(valueTest) <= 0 && 
                 valueUpper.compareTo(valueTest) >= 0);
      }
      else
      {
         // REVIEW: correct? Does this apply to boolean :-)
         return false;
      }
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String("(" +lower.toString() + ") <= (" + test.toString() +
                        ") <= (" + upper.toString()) + ")";
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
