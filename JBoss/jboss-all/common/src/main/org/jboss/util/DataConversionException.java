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
 * An exception throw to indicate a problem with some type of data conversion.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class DataConversionException
   extends NestedRuntimeException
{
   /**
    * Construct a <tt>DataConversionException</tt> with a specified detail message.
    *
    * @param msg     Detail message
    */
   public DataConversionException(final String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>DataConversionException</tt> with a specified detail Throwable
    * and message.
    *
    * @param msg     Detail message
    * @param detail  Detail Throwable
    */
   public DataConversionException(final String msg, final Throwable detail) {
      super(msg, detail);
   }

   /**
    * Construct a <tt>DataConversionException</tt> with a specified detail Throwable.
    *
    * @param detail  Detail Throwable
    */
   public DataConversionException(final Throwable detail) {
      super(detail);
   }

   /**
    * Construct a <tt>DataConversionException</tt> with no specified detail message.
    */
   public DataConversionException() {
      super();
   }
}
