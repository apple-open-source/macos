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
 * Thrown to indicate that a given attribute value is not valid.
 *
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version <tt>$Revision: 1.1 $</tt>
 */
public class InvalidAttributeException
   extends ConfigurationException
{
   /**
    * Construct a <tt>InvalidAttributeException</tt> with the 
    * specified detail message.
    *
    * @param name    The attribute name.
    * @param msg     The detail message.
    */
   public InvalidAttributeException(final String name, final String msg) {
      super(makeMessage(name, msg));
   }

   /**
    * Construct a <tt>InvalidAttributeException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param name    The attribute name.
    * @param msg     The detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public InvalidAttributeException(final String name, final String msg, final Throwable nested) 
   {
      super(makeMessage(name, msg), nested);
   }

   /**
    * Make a execption message for the attribute name and detail message.
    */
   private static String makeMessage(final String name, final String msg) {
      return "Invalid value for attribute '" + name + "': " + msg;
   }
}
