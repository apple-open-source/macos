/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.Externalizable;

import javax.jms.BytesMessage;
import javax.jms.JMSException;
import javax.jms.MessageEOFException;
import javax.jms.MessageFormatException;
import javax.jms.MessageNotWriteableException;

/**
 *  This class implements javax.jms.BytesMessage
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.8 $
 */
public class SpyBytesMessage
       extends SpyMessage
       implements Cloneable, BytesMessage, Externalizable {
   byte[]           InternalArray = null;

   // Attributes ----------------------------------------------------

   private transient ByteArrayOutputStream ostream = null;
   private transient DataOutputStream p = null;
   private transient ByteArrayInputStream istream = null;
   private transient DataInputStream m = null;

   private final static long serialVersionUID = -6572727147964701014L;

   // Constructor ---------------------------------------------------

   public SpyBytesMessage() {
      header.msgReadOnly = false;
      ostream = new ByteArrayOutputStream();
      p = new DataOutputStream( ostream );
   }

   // Public --------------------------------------------------------

   public boolean readBoolean()
      throws JMSException {
      checkRead();
      try {
         return m.readBoolean();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public byte readByte()
      throws JMSException {
      checkRead();
      try {
         return m.readByte();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public int readUnsignedByte()
      throws JMSException {
      checkRead();
      try {
         return m.readUnsignedByte();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public short readShort()
      throws JMSException {
      checkRead();
      try {
         return m.readShort();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public int readUnsignedShort()
      throws JMSException {
      checkRead();
      try {
         return m.readUnsignedShort();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public char readChar()
      throws JMSException {
      checkRead();
      try {
         return m.readChar();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public int readInt()
      throws JMSException {
      checkRead();
      try {
         return m.readInt();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public long readLong()
      throws JMSException {
      checkRead();
      try {
         return m.readLong();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public float readFloat()
      throws JMSException {
      checkRead();
      try {
         return m.readFloat();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public double readDouble()
      throws JMSException {
      checkRead();
      try {
         return m.readDouble();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public String readUTF()
      throws JMSException {
      checkRead();
      try {
         return m.readUTF();
      } catch ( EOFException e ) {
         throw new MessageEOFException( "" );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public int readBytes( byte[] value )
      throws JMSException {
      checkRead();
      try {
         return m.read( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public int readBytes( byte[] value, int length )
      throws JMSException {
      checkRead();
      try {
         return m.read( value, 0, length );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeBoolean( boolean value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeBoolean( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeByte( byte value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeByte( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeShort( short value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeShort( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeChar( char value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeChar( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeInt( int value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeInt( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeLong( long value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeLong( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeFloat( float value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeFloat( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeDouble( double value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeDouble( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeUTF( String value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.writeUTF( value );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeBytes( byte[] value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.write( value, 0, value.length );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeBytes( byte[] value, int offset, int length )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         p.write( value, offset, length );
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void writeObject( Object value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "the message body is read-only" );
      }
      try {
         if ( value instanceof String ) {
            p.writeChars( ( String )value );
         } else if ( value instanceof Boolean ) {
            p.writeBoolean( ( ( Boolean )value ).booleanValue() );
         } else if ( value instanceof Byte ) {
            p.writeByte( ( ( Byte )value ).byteValue() );
         } else if ( value instanceof Short ) {
            p.writeShort( ( ( Short )value ).shortValue() );
         } else if ( value instanceof Integer ) {
            p.writeInt( ( ( Integer )value ).intValue() );
         } else if ( value instanceof Long ) {
            p.writeLong( ( ( Long )value ).longValue() );
         } else if ( value instanceof Float ) {
            p.writeFloat( ( ( Float )value ).floatValue() );
         } else if ( value instanceof Double ) {
            p.writeDouble( ( ( Double )value ).doubleValue() );
         } else if ( value instanceof byte[] ) {
            p.write( ( byte[] )value, 0, ( ( byte[] )value ).length );
         } else {
            throw new MessageFormatException( "Invalid object for properties" );
         }
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }

   }

   public void reset()
      throws JMSException {
      try {
         if ( !header.msgReadOnly ) {
            p.flush();
            InternalArray = ostream.toByteArray();
            ostream.close();
         }
         ostream = null;
         istream = null;
         m = null;
         p = null;
         header.msgReadOnly = true;
      } catch ( IOException e ) {
         throw new JMSException( "IOException" );
      }
   }

   public void clearBody()
      throws JMSException {
      try {
         if ( !header.msgReadOnly ) {
            ostream.close();
         } else {
            // REVIEW: istream is only initialised on a read.
            // It looks like it is possible to acknowledge
            // a message without reading it? Guard against
            // an NPE in this case.
            if (istream != null)
              istream.close();
         }
      } catch ( IOException e ) {
         //don't throw an exception
      }

      ostream = new ByteArrayOutputStream();
      p = new DataOutputStream( ostream );
      InternalArray = null;
      istream = null;
      m = null;

      super.clearBody();
   }

   public SpyMessage myClone()
      throws JMSException {
      SpyBytesMessage result = MessagePool.getBytesMessage();
      this.reset();
      result.copyProps( this );
      if ( this.InternalArray != null ) {
         result.InternalArray = new byte[this.InternalArray.length];
         System.arraycopy( this.InternalArray, 0, result.InternalArray, 0, this.InternalArray.length );
      }
      return result;
   }

   public void writeExternal( java.io.ObjectOutput out )
      throws java.io.IOException {
      byte [] arrayToSend = null;
      if ( !header.msgReadOnly ) {
         p.flush();
         arrayToSend = ostream.toByteArray();
      }else{
        arrayToSend = InternalArray;
      }
      super.writeExternal( out );
      if ( arrayToSend == null ) {
         out.writeInt( 0 ); //pretend to be empty array
      } else {
         out.writeInt( arrayToSend.length );
         out.write( arrayToSend );
      }
   }

   public void readExternal( java.io.ObjectInput in )
      throws java.io.IOException, ClassNotFoundException {
      super.readExternal( in );
      int length = in.readInt();
      if ( length < 0 ) {
         InternalArray = null;
      } else {
         InternalArray = new byte[length];
         in.readFully( InternalArray );
      }
   }

   private void checkRead()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "readByte while the buffer is writeonly" );
      }

      //We have just received/reset() the message, and the client is trying to read it
      if ( istream == null || m == null ) {
         istream = new ByteArrayInputStream( InternalArray );
         m = new DataInputStream( istream );
      }
   }

   // JMS 1.1
   public long getBodyLength()
      throws JMSException
   {
      return InternalArray.length;
   }
}
/*
vim:ts=3:sw=3:et
*/
