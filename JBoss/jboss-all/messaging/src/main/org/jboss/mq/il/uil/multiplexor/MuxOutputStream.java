/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil.multiplexor;
import java.io.IOException;
import java.io.InterruptedIOException;

import java.io.OutputStream;

/**
 *  Objects of this class provide and an OutputStream to a StreamMux. Objects of
 *  this class are created by a StreamMux object.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
class MuxOutputStream extends OutputStream {

   StreamMux        streamMux;
   short            streamId;

   byte             buffer[];
   short            bufferLength;

   MuxOutputStream( StreamMux mux, short id ) {
      streamMux = mux;
      streamId = id;
      buffer = new byte[streamMux.frameSize];
      bufferLength = 0;
   }

   /**
    *  Closes this output stream and releases any system resources.
    *
    * @exception  IOException  Description of Exception
    */
   public void close()
      throws IOException {
      streamMux.closeStream( streamId );
   }

   /**
    *  Flushes this output stream and forces any buffered output bytes to be
    *  written out.
    *
    * @exception  IOException  Description of Exception
    */
   public void flush()
      throws IOException {
      if ( bufferLength > 0 ) {
         flushBuffer();
      }
      streamMux.flush();
   }

   /**
    * @param  data             Description of Parameter
    * @exception  IOException  Description of Exception
    */
   public void write( int data )
      throws IOException {
      byte b = ( byte )data;
      buffer[bufferLength] = b;
      bufferLength++;

      if ( bufferLength == streamMux.frameSize ) {
         flushBuffer();
      }
   }

   /**
    *  Flushes the internal buffer to the multiplexor.
    *
    * @exception  IOException  Description of Exception
    */
   private void flushBuffer()
      throws IOException {
      streamMux.write( this, buffer, bufferLength );
      bufferLength = 0;
   }
}
