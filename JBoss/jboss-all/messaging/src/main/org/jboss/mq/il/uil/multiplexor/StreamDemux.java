/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil.multiplexor;

import java.io.DataInputStream;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;

import EDU.oswego.cs.dl.util.concurrent.Mutex;

import org.jboss.logging.Logger;

/**
 *  This class is used to demultiplex from a single stream into multiple
 *  streams.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.3.4.1 $
 */
public class StreamDemux
{
   static Logger log = Logger.getLogger(StreamDemux.class);

   private short frameSize = 512;
   private HashMap openStreams = new HashMap();
   private InputStream in;
   private DataInputStream objectIn;
   private Mutex pumpMutex = new Mutex();
   private byte inputBuffer[] = new byte[frameSize];
   private boolean trace;

   /**
    *  StreamMux constructor comment.
    *
    * @param  in               Description of Parameter
    * @exception  IOException  Description of Exception
    */
   public StreamDemux( InputStream in )
      throws IOException
   {
      this.in = in;
      this.objectIn = new DataInputStream( in );
      this.trace = log.isTraceEnabled();
   }

   /**
    *  Creation date: (11/15/00 5:30:55 PM)
    *
    * @param  newFrameSize     short
    * @exception  IOException  Description of Exception
    */
   public void setFrameSize( short newFrameSize )
      throws IOException
   {
      synchronized ( openStreams )
      {
         if ( openStreams.size() > 0 )
         {
            throw new IOException( "Cannot change the frame size while there are open streams." );
         }
         frameSize = newFrameSize;
         inputBuffer = new byte[frameSize];
      }
   }

   /**
    *  Creation date: (11/15/00 5:30:55 PM)
    *
    * @return    short
    */
   public short getFrameSize()
   {
      synchronized ( openStreams )
      {
         return frameSize;
      }
   }

   public InputStream getStream( short id )
      throws IOException
   {
      if ( id == 0 )
      {
         throw new IOException( "Stream id 0 is reserved for internal use." );
      }
      
      InputStream s;
      synchronized ( openStreams )
      {
         s = ( InputStream )openStreams.get( new Short( id ) );
         if ( s != null )
         {
            return s;
         }

         s = new DemuxInputStream( this, id );
         openStreams.put( new Short( id ), s );
      }
      return s;
   }
   
   public int available( DemuxInputStream s )
      throws IOException
   {
      return objectIn.available();
   }

   public boolean attemptLock() throws InterruptedIOException
   {
      try
      {
         return pumpMutex.attempt(1);
      }
      catch(InterruptedException e)
      {
         throw new InterruptedIOException("Failed to acquire StreamDemux lock");
      }
   }
   public void releaseLock()
   {
      pumpMutex.release();
   }

   /**
    *  Pumps data to all input streams until data for the dest Stream arrives.
    *  Only on thread is allowed to pump data at a time and this method returns
    *  true if it pumped data into its input buffer. It returns false if another
    *  thread is allready pumping data.
    *
    * @param  dest             Description of Parameter
    * @exception  IOException  Description of Exception
    */
   public void pumpData( DemuxInputStream dest )
      throws IOException
   {
      if( trace )
         log.trace("pumpData, destStream: "+dest.streamId);

      // Start pumping the data
      boolean pumpingData = true;
      short nextFrameSize = frameSize;
      while ( pumpingData )
      {
         short streamId = objectIn.readShort();
         if( trace )
            log.trace("StreamId: "+streamId);
         // Was it a command on the admin stream?
         if ( streamId == 0 )
         {
            // Next byte is the command.
            switch ( objectIn.readByte() )
            {
               case StreamMux.OPEN_STREAM_COMMAND:
                  short openID = objectIn.readShort();
                  getStream( openID );
                  if( trace )
                     log.trace("Open stream: "+openID);
                  break;
               case StreamMux.CLOSE_STREAM_COMMAND:
                  DemuxInputStream s;
                  short closeID = objectIn.readShort();
                  synchronized ( openStreams )
                  {
                     s = ( DemuxInputStream )openStreams.get( new Short( closeID ) );
                  }
                  if ( s != null )
                  {
                     if( trace )
                        log.trace("Close stream: "+closeID);
                     s.atEOF = true;
                     closeStream( s.streamId );
                     if ( s == dest )
                     {
                        pumpingData = false;
                     }
                  }
                  break;
               case StreamMux.NEXT_FRAME_SHORT_COMMAND:
                  nextFrameSize = objectIn.readShort();
                  break;
            }
         }
         else
         {
            objectIn.readFully( inputBuffer, 0, nextFrameSize );
            DemuxInputStream s;
            synchronized ( openStreams )
            {
               s = ( DemuxInputStream )openStreams.get( new Short( streamId ) );
            }
            if ( s == null )
            {
               continue;
            }

            s.loadBuffer( inputBuffer, nextFrameSize );
            if( trace )
               log.trace("Loaded("+nextFrameSize+") bytes into: "+streamId);
            if ( s == dest )
            {
               break;
            }
            else
            {
               synchronized( this )
               {
                  notifyAll();
               }
            }
            nextFrameSize = frameSize;
         }
      }

   }

   void closeStream( short id )
      throws IOException
   {
      if ( id == 0 )
      {
         throw new IOException( "Stream id 0 is reserved for internal use." );
      }

      synchronized ( openStreams )
      {
         openStreams.remove( new Short( id ) );
      }
      synchronized( this )
      {
         notifyAll();
      }
   }
}
