/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * An In Query Expression.<p>
 *
 * Returns true only when any of the values are match.
 *
 * Returns true only when both expressions are true.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020315 Adrian Brock:</b>
 * <ul>
 * <li>Don't put ; on the end of if statements :-)
 * </ul>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3 $
 */
/*package*/ class InQueryExp
   extends QueryExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The value to test
    */
   ValueExp test;

   /**
    * The list of values 
    */
   ValueExp[] list;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Create a new IN query Expression
    * 
    * @param test the value to test
    * @param list the list of values
    */
   public InQueryExp(ValueExp test, ValueExp[] list)
   {
      this.test = test;
      this.list = list;
   }

   // Public ------------------------------------------------------

   // QueryExp implementation -------------------------------------

   public boolean apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      // REVIEW: Cast Exceptions
      ValueExp calcTest = test.apply(name);
      for (int i=0; i < list.length; i++)
      {
         ValueExp calcList = list[i].apply(name);
         // Number
         if (calcTest instanceof NumberValueExp)
         {
            if (((NumberValueExp)calcTest).getDoubleValue() ==
                ((NumberValueExp)calcList).getDoubleValue())
               return true;
         }
         // String
         else if (calcTest instanceof StringValueExp)
         {
            if (((StringValueExp)calcTest).toString().equals(
                ((StringValueExp)calcList).toString()))
               return true;
         }
         // Single Value, includes Boolean
         else if (calcTest instanceof SingleValueExpSupport)
         {
            if (((SingleValueExpSupport)calcTest).getValue().equals(
                ((SingleValueExpSupport)calcList).getValue()))
               return true;
         }
      }
      // No match
      return false;
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      StringBuffer buffer = new StringBuffer("(");
      buffer.append(test.toString());
      buffer.append(" in ");
      for (int i = 1; i < list.length; i++)
      {
        buffer.append(list[i].toString());
        buffer.append(" ");
      }
      buffer.append(")");
      return buffer.toString();
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
