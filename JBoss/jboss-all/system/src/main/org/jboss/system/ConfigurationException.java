/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import org.jboss.util.NestedException;

/**
 * Thrown to indicate a non-fatal configuration related problem.
 *
 * @see ConfigurationService
 *
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version <tt>$Revision: 1.1 $</tt>
 */
public class ConfigurationException
   extends NestedException
{
   /**
    * Construct a <tt>ConfigurationException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public ConfigurationException(String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>ConfigurationException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public ConfigurationException(String msg, Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>ConfigurationException</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public ConfigurationException(Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>ConfigurationException</tt> with no detail.
    */
   public ConfigurationException() {
      super();
   }
}
