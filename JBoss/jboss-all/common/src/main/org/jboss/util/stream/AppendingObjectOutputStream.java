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
 * An <tt>ObjectOutputStream</tt> that can conditionally be put into
 * <i>appending</i> mode.
 *
 * <dl>
 * <dt><b>Concurrency: </b></dt>
 * <dd>This class is <b>not</b> synchronized.</dd>
 * </dl>
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class AppendingObjectOutputStream
   extends ObjectOutputStreamAdapter
{
   /**
    * Construct an <tt>AppendingObjectOutputStream</tt>.
    *
    * @param out     An <tt>OutputStream</tt> stream.
    * @param append  <tt>True</tt> to append written objects; <tt>false</tt>
    *                to use default action (writes stream header).
    *
    * @throws IOException                 Any exception thrown by
    *                                     the underlying <tt>OutputStream</tt>.
    */
   public AppendingObjectOutputStream(OutputStream out, boolean append)
      throws IOException 
   {
      super(createStream(out, append));
   }

   /**
    * Helper to return a <tt>ObjectOutputStream</tt>.
    */
   private static ObjectOutputStream createStream(OutputStream out, 
                                                  boolean append)
      throws IOException
   {
      ObjectOutputStream stream;

      // if we are appending then return an append only stream
      if (append) {
         stream = new AppendObjectOutputStream(out);
      }
      // else if it already an oos then return it
      else if (out instanceof ObjectOutputStream) {
         stream = (ObjectOutputStream)out;
      }
      // else wrap the stream in an oos
      else {
         stream = new ObjectOutputStream(out);
      }

      return stream;
   }
}
