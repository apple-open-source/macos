/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * A Final Substring Query Expression.<p>
 *
 * Returns true when an attribute value ends with the string expression.
 *
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
/*package*/ class FinalSubStringQueryExp
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
    * Construct a new Final Substring query expression
    *
    * @param attr the attribute to test
    * @param string the string to test
    */
   public FinalSubStringQueryExp(AttributeValueExp attr, StringValueExp string)
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
         return ((StringValueExp)calcAttr).toString().endsWith(
                ((StringValueExp)calcString).toString());
      }
      // REVIEW: correct?
      return false;
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String("(" + attr.toString() + " finalSubString " +
                        string.toString() + ")");
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
