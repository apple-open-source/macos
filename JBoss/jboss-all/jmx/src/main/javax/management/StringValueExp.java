/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A String that is an arguement to a query.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2.6.1 $
 */
public class StringValueExp
   extends ValueExpSupport
{
   // Constants ---------------------------------------------------

   private static final long serialVersionUID = -3256390509806284044L;

   // Attributes --------------------------------------------------

   /**
    * The value of the string
    */
   private String val;

   // Static  -----------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a string value expression for the null string.
    */
   public StringValueExp()
   {
   }

   /**
    * Construct a string value expression for the passed string
    *
    * @param value the string
    */
   public StringValueExp(String value)
   {
      this.val = value;
   }

   // Public ------------------------------------------------------

   /**
    * Get the value of the string.
    *
    * @return the string value
    */
   public String getString()
   {
      return val;
   }

   // ValueExpSupport Overrides -----------------------------------

   // Object overrides --------------------------------------------

   public String toString()
   {
      return val;
   }

   // Protected ---------------------------------------------------

   // Package Private ---------------------------------------------

   // Private -----------------------------------------------------

   // Inner Classes -----------------------------------------------
}
