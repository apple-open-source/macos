/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * A buffered output stream that notifies every "chunk"
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:Adrian@jboss.org">Adrian Brock</a>
 */
public class NotifyingBufferedOutputStream
   extends BufferedOutputStream
{
   /**
    * The number of bytes between notifications
    */
   int chunkSize;

   /**
    * The number of bytes written in the current chunk
    */
   int chunk = 0;

   /**
    * The listener notified every chunk
    */
   StreamListener listener;

   /**
    * Construct a notifying buffered outputstream.<br>
    * The listener is notified once every chunk.
    *
    * @param os the output stream to be buffered
    * @param size the buffer size
    * @param chunkSize the chunk size
    * @exception IllegalArgumentException for a size <= 0 or chunkSize <= size or a null listener
    */
   public NotifyingBufferedOutputStream(OutputStream os, int size, int chunkSize, StreamListener listener)
   {
      super(os, size);
      if (chunkSize <= size)
         throw new IllegalArgumentException("chunkSize must be bigger than the buffer");
      this.chunkSize = chunkSize;
      this.listener = listener;
   }

   public void setStreamListener(StreamListener listener)
   {
      this.listener = listener;
   }

   public void write(int b)
      throws IOException
   {
      super.write(b);
      checkNotification(1);
   }

   public void write(byte[] b, int off, int len)
      throws IOException
   {
      super.write(b, off, len);
      checkNotification(len);
   }

   /**
    * Checks whether a notification is required and
    * notifies as appropriate
    *
    * @param result the number of bytes written
    */
   public void checkNotification(int result)
   {
      // Is a notification required?
      chunk += result;
      if (chunk >= chunkSize)
      {
         if (listener != null)
            listener.onStreamNotification(this, chunk);

         // Start a new chunk
         chunk = 0;
      }
   }
}
