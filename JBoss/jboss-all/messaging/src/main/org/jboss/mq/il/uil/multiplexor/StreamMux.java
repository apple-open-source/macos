/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil.multiplexor;

import java.io.DataOutputStream;
import java.io.OutputStream;
import java.io.IOException;

import java.util.HashMap;

/**
 *  This class is used to multiplex from multiple streams into a single stream.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.3 $
 */
public class StreamMux {

   short            frameSize = 512;
   HashMap          openStreams = new HashMap();
   OutputStream     out;
   DataOutputStream objectOut;

   // Commands that can be sent over the admin stream.
   final static byte OPEN_STREAM_COMMAND = 0;
   final static byte CLOSE_STREAM_COMMAND = 1;
   final static byte NEXT_FRAME_SHORT_COMMAND = 2;

   /**
    *  StreamMux constructor comment.
    *
    * @param  out              java.io.OutputStream
    * @exception  IOException  Description of Exception
    */
   public StreamMux( OutputStream out )
      throws IOException {
      this.out = out;
      this.objectOut = new DataOutputStream( out );
   }

   /**
    *  Insert the method's description here. Creation date: (11/15/00 5:30:55
    *  PM)
    *
    * @param  newFrameSize     short
    * @exception  IOException  Description of Exception
    */
   public void setFrameSize( short newFrameSize )
      throws IOException {
      synchronized ( openStreams ) {
         if ( openStreams.size() > 0 ) {
            throw new IOException( "Cannot change the frame size while there are open streams." );
         }
         frameSize = newFrameSize;
      }
   }

   /**
    *  Insert the method's description here. Creation date: (11/15/00 5:30:55
    *  PM)
    *
    * @return    short
    */
   public short getFrameSize() {
      synchronized ( openStreams ) {
         return frameSize;
      }
   }

   public OutputStream getStream( short id )
      throws IOException {
      if ( id == 0 ) {
         throw new IOException( "Stream id 0 is reserved for internal use." );
      }

      OutputStream s;
      synchronized ( openStreams ) {
         s = ( OutputStream )openStreams.get( new Short( id ) );
         if ( s != null ) {
            return s;
         }

         s = new MuxOutputStream( this, id );
         openStreams.put( new Short( id ), s );
      }

      synchronized ( objectOut ) {
         objectOut.writeShort( 0 );
         // admin stream
         objectOut.writeByte( OPEN_STREAM_COMMAND );
         // command
         objectOut.writeShort( id );
         // argument
      }
      return s;
   }

   public void flush()
      throws IOException {

      synchronized ( objectOut ) {
         objectOut.flush();
         out.flush();
      }

   }

   void closeStream( short id )
      throws IOException {
      if ( id == 0 ) {
         throw new IOException( "Stream id 0 is reserved for internal use." );
      }

      MuxOutputStream s;

      synchronized ( openStreams ) {
         s = ( MuxOutputStream )openStreams.remove( new Short( id ) );
      }

      synchronized ( objectOut ) {
         objectOut.writeShort( 0 );
         // admin stream
         objectOut.writeByte( CLOSE_STREAM_COMMAND );
         // command
         objectOut.writeShort( id );
         // argument
      }
   }

   void write( MuxOutputStream s, byte b[], int len )
      throws IOException {

      if ( b == null ) {
         throw new NullPointerException();
      } else if ( ( len < 0 ) || ( len > b.length ) || ( len > frameSize ) ) {
         throw new IndexOutOfBoundsException();
      } else if ( len == 0 ) {
         return;
      }

      synchronized ( objectOut ) {
         if ( len < frameSize ) {
            objectOut.writeShort( 0 );
            objectOut.writeByte( NEXT_FRAME_SHORT_COMMAND );
            objectOut.writeShort( len );
         }
         objectOut.writeShort( s.streamId );
         objectOut.write( b, 0, len );
      }
   }
}
