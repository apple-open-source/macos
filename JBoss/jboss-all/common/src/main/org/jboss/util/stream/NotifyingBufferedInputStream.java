/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.BufferedInputStream;
import java.io.InputStream;
import java.io.IOException;

/**
 * A buffered input stream that notifies every "chunk"
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:Adrian@jboss.org">Adrian Brock</a>
 */
public class NotifyingBufferedInputStream
   extends BufferedInputStream
{
   /**
    * The number of bytes between notifications
    */
   int chunkSize;

   /**
    * The number of bytes read in the current chunk
    */
   int chunk = 0;

   /**
    * The listener notified every chunk
    */
   StreamListener listener;

   /**
    * Construct a notifying buffered inputstream.
    * The listener is notified once every chunk.
    *
    * @param is the input stream to be buffered
    * @param size the buffer size
    * @param chunkSize the chunk size
    * @param listener the listener to notify
    * @exception IllegalArgumentException for a size <= 0 or chunkSize <= size
    */
   public NotifyingBufferedInputStream(InputStream is, int size, int chunkSize, StreamListener listener)
   {
      super(is, size);
      if (chunkSize <= size)
         throw new IllegalArgumentException("chunkSize must be bigger than the buffer");
      this.chunkSize = chunkSize;
      this.listener = listener;
   }

   public void setStreamListener(StreamListener listener)
   {
      this.listener = listener;
   }

   public int read()
      throws IOException
   {
      int result = super.read();
      if (result == -1)
         return result;
      checkNotification(result);
      return result;
   }

   public int read(byte[] b, int off, int len)
      throws IOException
   {
      int result = super.read(b, off, len);
      if (result == -1)
         return result;
      checkNotification(result);
      return result;
   }

   /**
    * Checks whether a notification is required and
    * notifies as appropriate
    *
    * @param result the number of bytes read
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
