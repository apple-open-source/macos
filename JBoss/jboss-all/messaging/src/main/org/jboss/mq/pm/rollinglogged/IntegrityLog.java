/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.rollinglogged;

import java.io.File;
import java.io.RandomAccessFile;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ObjectInput;
import java.io.ObjectOutput;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.io.EOFException;
import java.io.IOException;

import org.jboss.mq.SpyMessage;

/**
 *  This class is used to create a log file which which will will garantee it's
 *  integrety up to the last commit point. An optimised version of the
 *  integrityLog in the logged persistence.
 *
 * @created    August 16, 2001
 * @author:    David Maplesden (David.Maplesden@orion.co.nz)
 * @version    $Revision: 1.4.4.1 $
 */
public class IntegrityLog {

   /////////////////////////////////////////////////////////////////////
   // Attributes
   /////////////////////////////////////////////////////////////////////

   private RandomAccessFile raf;
   private File     f;
   private ObjectOutput objectOutput;

   protected final static byte TX = 0;
   protected final static byte ADD = 1;
   protected final static byte REMOVE = 2;
   protected final static byte UPDATE = 3;

   /////////////////////////////////////////////////////////////////////
   // Constructor
   /////////////////////////////////////////////////////////////////////
   public IntegrityLog( File file )
      throws IOException {
      f = file;
      raf = new RandomAccessFile( f, "rw" );
      this.objectOutput = new MyObjectOutputStream( new MyOutputStream() );
      seekEnd();
   }

   /////////////////////////////////////////////////////////////////////
   // Public Methods
   /////////////////////////////////////////////////////////////////////

   public void commit()
      throws IOException {
      //raf.getFD().sync();
   }

   public void delete()
      throws IOException {
      f.delete();
   }

   public void close()
      throws IOException {
      raf.close();
      raf = null;
   }


   public synchronized void add( long messageID, boolean isTransacted, long txId, SpyMessage message )
      throws IOException {
      raf.writeByte( ADD );
      raf.writeLong( messageID );
      raf.writeBoolean( isTransacted );
      raf.writeLong( txId );
      SpyMessage.writeMessage( message, objectOutput );
      objectOutput.flush();
   }

   public synchronized void remove( long messageID, boolean isTransacted, long txId )
      throws IOException {
      raf.writeByte( REMOVE );
      raf.writeLong( messageID );
      raf.writeBoolean( isTransacted );
      raf.writeLong( txId );
      objectOutput.flush();
   }

   public synchronized void update( long messageID, boolean isTransacted, long txId, SpyMessage message )
      throws IOException
   {
      raf.writeByte( UPDATE );
      raf.writeLong( messageID );
      raf.writeBoolean( isTransacted );
      raf.writeLong( txId );
      SpyMessage.writeMessage( message, objectOutput );
      objectOutput.flush();
   }

   public void skipNextEntry( ObjectInput in )
      throws IOException {
      byte type = raf.readByte();
      switch ( type ) {
         case TX:
            raf.readLong();
            return;
         case ADD:
            raf.readLong();
            raf.readBoolean();
            raf.readLong();
            SpyMessage.readMessage( in );
            return;
         case REMOVE:
            raf.readLong();
            raf.readBoolean();
            raf.readLong();
            return;
         case UPDATE:
            raf.readLong();
            raf.readBoolean();
            raf.readLong();
            SpyMessage.readMessage( in );
            return;
         default:
            throw new java.io.IOException( "Error in log file format." );
      }
   }

   public java.util.LinkedList toIndex()
      throws IOException {
      raf.seek( 0 );
      long length = raf.length();
      long pos = 0;
      ObjectInput in = new MyObjectInputStream( new MyInputStream() );
      java.util.LinkedList ll = new java.util.LinkedList();
      try {
         while ( pos < length ) {
            ll.add( readNextEntry( in ) );
            pos = raf.getFilePointer();
         }
      } catch ( EOFException e ) {
         //incomplete record
      }
      in.close();
      raf.seek( pos );

      return ll;
   }

   public java.util.TreeSet toTreeSet()
      throws IOException {
      raf.seek( 0 );
      long length = raf.length();
      long pos = 0;
      ObjectInput in = new MyObjectInputStream( new MyInputStream() );
      java.util.TreeSet ll = new java.util.TreeSet();
      try {
         while ( pos < length ) {
            ll.add( readNextEntry( in ) );
            pos = raf.getFilePointer();
         }
      } catch ( EOFException e ) {
         //incomplete record
      }
      in.close();
      raf.seek( pos );

      return ll;
   }

   public Object readNextEntry( ObjectInput in )
      throws IOException {
      byte type = raf.readByte();
      switch ( type ) {
         case TX:
            return new org.jboss.mq.pm.Tx( raf.readLong() );
         case ADD:
            MessageAddedRecord add = new MessageAddedRecord();
            add.messageId = raf.readLong();
            add.isTransacted = raf.readBoolean();
            add.transactionId = raf.readLong();
            add.message = SpyMessage.readMessage( in );
            return add;
         case REMOVE:
            MessageRemovedRecord remove = new MessageRemovedRecord();
            remove.messageId = raf.readLong();
            remove.isTransacted = raf.readBoolean();
            remove.transactionId = raf.readLong();
            return remove;
         case UPDATE:
            MessageUpdateRecord update = new MessageUpdateRecord();
            update.messageId = raf.readLong();
            update.isTransacted = raf.readBoolean();
            update.transactionId = raf.readLong();
            update.message = SpyMessage.readMessage( in );
            return update;
         default:
            throw new java.io.IOException( "Error in log file format." );
      }
   }

   public synchronized void addTx( org.jboss.mq.pm.Tx tx )
      throws IOException {
      raf.writeByte( TX );
      raf.writeLong( tx.longValue() );
      objectOutput.flush();
   }

   private void seekEnd()
      throws IOException {
      raf.seek( 0 );
      long length = raf.length();
      long pos = 0;
      ObjectInput in = new MyObjectInputStream( new MyInputStream() );
      try {
         while ( pos < length ) {
            skipNextEntry( in );
            pos = raf.getFilePointer();
         }
      } catch ( EOFException e ) {
         //incomplete record, must have been due to program failure during write.
      }
      in.close();
      raf.seek( pos );
   }

   ////////////////////////////////////////////////////////////
   //  Helper Inner classes.                                 //
   ////////////////////////////////////////////////////////////

   /**
    * @created    August 16, 2001
    */
   class MyOutputStream extends OutputStream {
      public void close()
         throws IOException {
         flush();
      }

      public void write( int b )
         throws IOException {
         raf.write( ( byte )b );
      }

      public void write( byte bytes[], int off, int len )
         throws IOException {
         raf.write( bytes, off, len );
      }
   }

   /**
    * @created    August 16, 2001
    */
   class MyObjectOutputStream extends ObjectOutputStream {
      MyObjectOutputStream( OutputStream os )
         throws IOException {
         super( os );
      }

      protected void writeStreamHeader() {
      }
   }

   /**
    * @created    August 16, 2001
    */
   class MyObjectInputStream extends ObjectInputStream {
      MyObjectInputStream( InputStream is )
         throws IOException {
         super( is );
      }

      protected void readStreamHeader() {
      }
   }

   /**
    * @created    August 16, 2001
    */
   class MyInputStream extends InputStream {
      public void close()
         throws IOException {
      }

      public int read()
         throws IOException {
         return raf.read();
      }

      public int read( byte bytes[], int off, int len )
         throws IOException {
         return raf.read( bytes, off, len );
      }
   }

   /**
    * @created    August 16, 2001
    */
   class MessageAddedRecord implements Serializable {
      long          messageId;
      boolean       isTransacted;
      long          transactionId;
      SpyMessage    message;
      private final static long serialVersionUID = 235726945332013954L;
   }

   /**
    * @created    August 16, 2001
    */
   class MessageRemovedRecord implements Serializable {
      boolean       isTransacted;
      long          transactionId;
      long          messageId;
      private final static long serialVersionUID = 235726945332013955L;
   }

   class MessageUpdateRecord implements Serializable {
      boolean       isTransacted;
      long          transactionId;
      long          messageId;
      SpyMessage    message;
      private final static long serialVersionUID = 235726945332013956L;
   }
}
