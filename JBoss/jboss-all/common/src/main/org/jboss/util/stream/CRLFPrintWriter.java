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
import java.io.PrintWriter;
import java.io.OutputStream;
import java.io.InterruptedIOException;
import java.io.Writer;

/**
 * A <tt>PrintWriter</tt> that ends lines with a carriage return-line feed 
 * (<tt>CRLF</tt>).
 *
 * <h3>Concurrency</h3>
 * This class is <b>as</b> synchronized as <tt>PrintWriter</tt>.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CRLFPrintWriter
   extends PrintWriter
{
   protected boolean autoFlush = false;

   public CRLFPrintWriter(final Writer out) {
      super(out);
   }

   public CRLFPrintWriter(final Writer out, final boolean autoFlush) {
      super(out, autoFlush);
      this.autoFlush = autoFlush;
   }

   public CRLFPrintWriter(final OutputStream out) {
      super(out);
   }

   public CRLFPrintWriter(final OutputStream out, final boolean autoFlush) {
      super(out, autoFlush);
      this.autoFlush = autoFlush;
   }

   protected void ensureOpen() throws IOException {
      if (out == null)
         throw new IOException("Stream closed");
   }

   public void println() {
      try {
         synchronized (lock) {
            ensureOpen();

            out.write("\r\n");

            if (autoFlush) {
               out.flush();
            }
         }
      }
      catch (InterruptedIOException e) {
         Thread.currentThread().interrupt();
      }
      catch (IOException e) {
         setError();
      }
   }      
}
