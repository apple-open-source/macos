/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

/**
 * A simple interface for objects that can return pretty (ie.
 * prefixed) string representations of themselves.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface PrettyString
{
   /**
    * Returns a pretty representation of the object.
    *
    * @param prefix  The string which all lines of the output must be prefixed with.
    * @return        A pretty representation of the object.
    */
   String toPrettyString(String prefix);

   /**
    * Interface for appending the objects pretty string onto a buffer.
    */
   interface Appendable
   {
      /**
       * Appends a pretty representation of the object to the given buffer.
       *
       * @param buff    The buffer to use while making pretty.
       * @param prefix  The string which all lines of the output must be prefixed with.
       * @return        The buffer.
       */
      StringBuffer appendPrettyString(StringBuffer buff, String prefix);
   }
}
