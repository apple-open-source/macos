/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil.multiplexor;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InterruptedIOException;

import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;

/**
 *  This class is a unit tester of the StreamMux and StreamDemux classes. Starts
 *  3 concurent readers and 3 concurent writers using 3 fully buffered object
 *  streams multiplexed over a single Pipe(Input/Output)Stream. The writers send
 *  a 10K message with a timestamp. Readers display how long the message took to
 *  arrive.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class MultiplexorTest {

   StreamMux        mux;
   StreamDemux      demux;

   public final static int PAY_LOAD_SIZE = 1024 * 10;
   public static char[] PAY_LOAD;

   /**
    *  MuxDemuxTester constructor
    */
   public MultiplexorTest() {
      super();

      char s[] = new char[PAY_LOAD_SIZE];
      char c = 'A';
      for ( int i = 0; i < PAY_LOAD_SIZE; i++ ) {
         s[i] = c;
         c++;
         c = c > 'Z' ? 'A' : c;
      }
      PAY_LOAD = s;
   }

   public void connect()
      throws IOException {
      PipedInputStream pis = new PipedInputStream();
      PipedOutputStream pos = new PipedOutputStream( pis );

      mux = new StreamMux( pos );
      demux = new StreamDemux( pis );
   }

   public void startStream( short id )
      throws IOException {

      new WriterThread( id ).start();
      new ReaderThread( id ).start();

   }

   public static void main( String args[] )
      throws Exception {

      System.out.println( "Initializing" );
      MultiplexorTest tester = new MultiplexorTest();

      System.out.println( "Connecting the streams" );
      tester.connect();
      System.out.println( "Starting stream 1" );
      tester.startStream( ( short )1 );
      System.out.println( "Starting stream 2" );
      tester.startStream( ( short )2 );
      System.out.println( "Starting stream 3" );
      tester.startStream( ( short )3 );

   }

   /**
    * @created    August 16, 2001
    */
   class WriterThread extends Thread {
      ObjectOutputStream os;
      short         id;

      WriterThread( short id )
         throws IOException {
         super( "WriterThread" );
         this.os = new ObjectOutputStream( new BufferedOutputStream( mux.getStream( id ) ) );
         this.os.flush();
      }

      public void run() {
         try {
            for ( int i = 0; i < 1000; i++ ) {
               os.writeLong( System.currentTimeMillis() );
               os.writeObject( PAY_LOAD );
               os.flush();
            }
         } catch ( IOException e ) {
            e.printStackTrace();
         }
      }
   }

   /**
    * @created    August 16, 2001
    */
   class ReaderThread extends Thread {
      ObjectInputStream is;
      short         id;

      ReaderThread( short id )
         throws IOException {
         super( "ReaderThread" );
         this.is = new ObjectInputStream( new BufferedInputStream( demux.getStream( id ) ) );
         this.id = id;
      }

      public void run() {
         try {
            for ( int i = 0; i < 1000; i++ ) {
               long t = is.readLong();
               is.readObject();
               t = System.currentTimeMillis() - t;
               System.out.println( "" + id + ": Packet " + i + " Latency : " + ( ( double )t / ( double )1000 ) );
               System.out.flush();
            }
         } catch ( Exception e ) {
            e.printStackTrace();
         }
      }
   }
}
