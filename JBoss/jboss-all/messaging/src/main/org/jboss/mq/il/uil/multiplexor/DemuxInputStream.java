/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil.multiplexor;

import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.IOException;

/**
 *  Objects of this class provide and an InputStream from a StreamDemux. Objects
 *  of this class are created by a StreamDemux object.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2.8.1 $
 */
class DemuxInputStream extends InputStream
{
   StreamDemux streamDemux;
   short streamId;
   boolean atEOF = false;
   DynCircularBuffer buffer;

   DemuxInputStream( StreamDemux demux, short id )
   {
      streamDemux = demux;
      streamId = id;
      buffer = new DynCircularBuffer(1024);
   }

   public int available()
      throws IOException
   {
      return buffer.getSize();
   }

   public void close()
      throws IOException
   {
      streamDemux.closeStream( streamId );
   }

   public void loadBuffer( byte data[], short dataLength )
      throws IOException
   {
      buffer.fill(data, dataLength);
   }

   public synchronized int read()
      throws IOException
   {
      if ( atEOF )
         return -1;

      if( buffer.getSize() == 0 )
      {
         fillBuffer();
      }
      // We might break out due to EOF
      if ( atEOF )
         return -1;

      int result = buffer.get();
      return result & 0xff;
   }

   public synchronized int read( byte b[], int off, int len )
      throws IOException
   {
      if ( atEOF )
         return -1;
      if ( b == null )
      {
         throw new NullPointerException();
      }
      else if ( ( off < 0 ) || ( off > b.length ) || ( len < 0 ) ||
         ( ( off + len ) > b.length ) || ( ( off + len ) < 0 ) )
      {
         throw new IndexOutOfBoundsException();
      }
      else if ( len == 0 )
      {
         return 0;
      }

      if( buffer.getSize() == 0 )
      {
         fillBuffer();
      }
      // We might break out due to EOF
      if ( atEOF )
         return -1;

      int bytesRead = buffer.get(b, off, len);
      return bytesRead;
   }

   private void fillBuffer() throws IOException
   {
      boolean acquired = false;
      try
      {
         synchronized( streamDemux )
         {
            while( atEOF == false && acquired == false && buffer.getSize() == 0 )
            {
               acquired = streamDemux.attemptLock();
               if( acquired == false )
               {
                  try
                  {
                     streamDemux.wait();
                  }
                  catch(InterruptedException e)
                  {
                     throw new InterruptedIOException("Interrupted waiting for StreamDemux");
                  }
               }
            }
         }
         if( acquired == true )
         {
            streamDemux.pumpData(this);
         }
      }
      finally
      {
         if( acquired == true )
         {
            streamDemux.releaseLock();
            /* We must notify any waiting threads in the event that we only
               saw data for this stream
            */
            synchronized( streamDemux )
            {
               streamDemux.notifyAll();
            }
         }
      }
   }
}
