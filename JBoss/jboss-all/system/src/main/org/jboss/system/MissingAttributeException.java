/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

/**
 * Thrown to indicate that a required attribute has not been set.
 *
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version <tt>$Revision: 1.1 $</tt>
 */
public class MissingAttributeException
   extends ConfigurationException
{
   /**
    * Construct a <tt>MissingAttributeException</tt> with the specified detail 
    * message.
    *
    * @param name    The attribute name.
    */
   public MissingAttributeException(final String name) {
      super(makeMessage(name));
   }

   /**
    * Construct a <tt>MissingAttributeException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param name    The attribute name.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public MissingAttributeException(final String name, final Throwable nested) {
      super(makeMessage(name), nested);
   }

   /**
    * Make a execption message for the attribute name.
    */
   private static String makeMessage(final String name) {
      return "Missing attribute '" + name + "'";
   }
}
