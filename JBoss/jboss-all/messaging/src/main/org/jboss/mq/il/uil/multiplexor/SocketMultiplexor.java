/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil.multiplexor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.net.Socket;

/**
 *  Used to multiplex a socket's streams. With this this interface you can
 *  access the multiplexed streams of the socket. The multiplexed streams are
 *  identifed a stream id. Stream id 0 is reserved for internal use of the
 *  multiplexor.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class SocketMultiplexor {
   StreamMux        mux;
   StreamDemux      demux;

   private Socket   socket;

   public SocketMultiplexor( Socket s )
      throws IOException {
      socket = s;
      mux = new StreamMux( s.getOutputStream() );
      demux = new StreamDemux( s.getInputStream() );
   }

   /**
    *  Creation date: (11/16/00 1:15:01 PM)
    *
    * @return    org.jboss.mq.connection.StreamDemux
    */
   public StreamDemux getDemux() {
      return demux;
   }

   public InputStream getInputStream( int id )
      throws IOException {
      return demux.getStream( ( short )id );
   }

   /**
    *  Creation date: (11/16/00 1:15:01 PM)
    *
    * @return    org.jboss.mq.connection.StreamMux
    */
   public StreamMux getMux() {
      return mux;
   }

   public OutputStream getOutputStream( int id )
      throws IOException {
      return mux.getStream( ( short )id );
   }

   /**
    *  Creation date: (11/16/00 1:14:41 PM)
    *
    * @return    java.net.Socket
    */
   public java.net.Socket getSocket() {
      return socket;
   }

   public void close()
      throws IOException {
      getSocket().close();
   }

}
