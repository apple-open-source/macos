/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * An Any Substring Query Expression.<p>
 *
 * Returns true when an attribute value contains the string expression.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
/*package*/ class AnySubStringQueryExp
   extends QueryExpSupport
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The attribute to test
    */
   AttributeValueExp attr;

   /**
    * The string to test
    */
   StringValueExp string;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a new Any Substring query expression
    *
    * @param attr the attribute to test
    * @param string the string to test
    */
   public AnySubStringQueryExp(AttributeValueExp attr, StringValueExp string)
   {
      this.attr = attr;
      this.string = string;
   }

   // Public ------------------------------------------------------

   // QueryExp implementation -------------------------------------

   public boolean apply(ObjectName name)
      throws BadStringOperationException,
             BadBinaryOpValueExpException,
             BadAttributeValueExpException,
             InvalidApplicationException
   {
      ValueExp calcAttr = attr.apply(name);
      ValueExp calcString = string.apply(name);
      if (calcAttr instanceof StringValueExp)
      {
         return ((StringValueExp)calcAttr).toString().indexOf(
                ((StringValueExp)calcString).toString()) != -1;
      }
      // REVIEW: correct?
      return false;
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String("(" + attr.toString() + " anySubString " +
                        string.toString() + ")");
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
