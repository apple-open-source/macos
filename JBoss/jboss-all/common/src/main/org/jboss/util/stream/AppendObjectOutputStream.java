/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.OutputStream;

/**
 * An <code>ObjectOutputStream</code> that is meant for appending onto an
 * existing stream written to by a non <code>AppendObjectOutputStream</code>
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class AppendObjectOutputStream
   extends ObjectOutputStream
{
   /**
    * Construct a new AppendObjectOutputStream.
    *
    * @param out     An output stream.
    *
    * @throws IOException  Any exception thrown by the underlying OutputStream.
    */
   public AppendObjectOutputStream(OutputStream out) throws IOException {
      super(out);
   }

   /**
    * Reset the stream, does not write headers.
    *
    * @throws IOException  Any exception thrown by the underlying OutputStream.
    */
   protected void writeStreamHeader() throws IOException {
      this.reset();
   }
}
