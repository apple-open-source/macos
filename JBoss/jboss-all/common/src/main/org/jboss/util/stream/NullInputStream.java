/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.InputStream;

/**
 * A <tt>null</tt> <code>InputStream</code>.  Methods that return values, 
 * return values that indicate that there is no more data to be read, other 
 * methods are non-operations.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class NullInputStream
   extends InputStream
{
   /** A default null input stream. */
   public static final NullInputStream INSTANCE = new NullInputStream();

   /**
    * Always returns zero.
    *
    * @return  Zero.
    */
   public int available() {
      return 0;
   }

   /**
    * Non-operation.
    */
   public void mark(final int readLimit) {
   }

   /**
    * Always returns false.
    *
    * @return  False.
    */
   public boolean markSupported() {
      return false;
   }

   /**
    * Non-operation.
    */
   public void reset() {
   }

   /**
    * Non-operation.
    */
   public void close() {
   }

   /**
    * Always returns -1.
    *
    * @return  -1.
    */
   public int read() {
      return -1;
   }

   /**
    * Always returns -1.
    *
    * @return  -1.
    */
   public int read(final byte bytes[], final int offset, final int length) {
      return -1;
   }

   /**
    * Always returns -1.
    *
    * @return  -1.
    */
   public int read(final byte bytes[]) {
      return -1;
   }

   /**
    * Always returns zero.
    *
    * @return  Zero.
    */
   public long skip(final long n) {
      return 0;
   }
}
