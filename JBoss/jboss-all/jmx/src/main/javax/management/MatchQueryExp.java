/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import gnu.regexp.UncheckedRE;

/**
 * A Match Query Expression.<p>
 *
 * Returns true when an attribute value matches the string expression.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020314 Adrian Brock:</b>
 * <ul>
 * <li>Fixed most of the escaping
 * </ul>
 * <p><b>20020317 Adrian Brock:</b>
 * <ul>
 * <li>Make queries thread safe
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.4 $
 */
/*package*/ class MatchQueryExp
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

   /**
    * Regular Expression
    */
   UncheckedRE re;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a new MATCH query expression
    *
    * @param attr the attribute to test
    * @param string the string to test
    */
   public MatchQueryExp(AttributeValueExp attr, StringValueExp string)
   {
      this.attr = attr;
      this.string = string;

      // Translate the pattern to a regexp
      StringBuffer buffer = new StringBuffer();
      char[] chars = string.toString().toCharArray();
      boolean escaping = false;
      for (int i=0; i < chars.length; i++)
      {
         // Turn on escaping
         if (chars[i] == '\\' && escaping == false)
            escaping = true;
         else
         {
            // Match any character
            if (chars[i] == '?' && escaping == false)
               buffer.append("(?:.)");
            // A literal question mark
            else if (chars[i] == '?')
               buffer.append("\\?");
            // Match any number of characters including none
            else if (chars[i] == '*' && escaping == false)
               buffer.append("(?:.)*");
            // A literal asterisk
            else if (chars[i] == '*')
               buffer.append("\\*");
            // The hat character is literal
            else if (chars[i] == '^')
               buffer.append("\\^");
            // The dollar sign is literal
            else if (chars[i] == '$')
               buffer.append("\\$");
            // The back slash character is literal (avoids escaping)
            else if (chars[i] == '\\')
               buffer.append("\\\\");
            // The dot character is literal
            else if (chars[i] == '.')
               buffer.append("\\.");
            // The vertical line character is literal
            else if (chars[i] == '|')
               buffer.append("\\|");
            // Escaping the open bracket
            else if (chars[i] == '[' && escaping == true)
               buffer.append("\\[");
            // REVIEW: There are other more complicated expressions to escape
            else
               buffer.append(chars[i]);
            escaping = false;
         }
      }
      // REVIEW: Should this be an error?
      if (escaping == true)
         buffer.append("\\\\");
         
      re = new UncheckedRE(buffer);
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
         return re.isMatch(((StringValueExp)calcAttr).toString());
      }
      // Correct?
      return false;
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      return new String("(" + attr.toString() + " matches " +
                        string.toString() + ")");
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
