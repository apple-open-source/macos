/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.OutputStream;

/**
 * A <tt>null</tt> <code>OutputStream</code>.  All values passed to 
 * {@link #write(int)} are discarded.  Calls to {@link #flush()} and 
 * {@link #close()} are ignored. 
 *
 * <p>All methods are declared <b>NOT</b> to throw <code>IOException</code>s.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class NullOutputStream
   extends OutputStream
{
   /** A default null output stream. */
   public static final NullOutputStream STREAM = new NullOutputStream();

   /**
    * Non-operation.
    */
   public void write(final int b) {}

   /**
    * Non-operation.
    */
   public void flush() {}

   /**
    * Non-operation.
    */
   public void close() {}

   /**
    * Non-operation.
    */
   public void write(final byte[] bytes) {}

   /**
    * Non-operation.
    */
   public void write(final byte[] bytes, final int offset, final int length) {}
}
