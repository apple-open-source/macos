/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * A NOT Query Expression.<p>
 *
 * Returns true when either expression is false.
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
/*package*/ class NotQueryExp
   extends QueryExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The query expression to negate
    */
   QueryExp expression;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------


   /**
    * Create a new NOT query Expression
    * 
    * @param expression the query expression to negate
    */
   public NotQueryExp(QueryExp expression)
   {
      this.expression = expression;
   }

   // Public ------------------------------------------------------

   // QueryExp implementation -------------------------------------

   public boolean apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      return !expression.apply(name); 
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String("!(" + expression.toString() + ")" );
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
