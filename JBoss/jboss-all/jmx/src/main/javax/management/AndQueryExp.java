/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * An And Query Expression.<p>
 *
 * Returns true only when both expressions are true.
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
/*package*/ class AndQueryExp
   extends QueryExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The first query expression
    */
   QueryExp first;

   /**
    * The second query expression
    */
   QueryExp second;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Create a new AND query Expression
    * 
    * @param first the first query expression
    * @param second the second query expression
    */
   public AndQueryExp(QueryExp first, QueryExp second)
   {
      this.first = first;
      this.second = second;
   }

   // Public ------------------------------------------------------

   // QueryExp implementation -------------------------------------

   public boolean apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      return first.apply(name) && second.apply(name); 
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String("(" +first.toString() + ") && (" + second.toString()) + ")";
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
