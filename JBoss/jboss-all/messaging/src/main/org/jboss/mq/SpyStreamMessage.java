/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Externalizable;

import java.util.Vector;
import javax.jms.JMSException;
import javax.jms.MessageEOFException;
import javax.jms.MessageFormatException;
import javax.jms.MessageNotWriteableException;

import javax.jms.StreamMessage;

/**
 *  This class implements javax.jms.StreamMessage
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.5 $
 */
public class SpyStreamMessage
       extends SpyMessage
       implements StreamMessage, Cloneable, Externalizable {
   // Attributes ----------------------------------------------------

   Vector           content;
   int              position;
   int              offset;
   int              size;

   private final static long serialVersionUID = 2490910971426786841L;

   // Constructor ---------------------------------------------------

   public SpyStreamMessage() {
      content = new Vector();
      position = 0;
      size = 0;
      offset = 0;
   }

   // Public --------------------------------------------------------

   public boolean readBoolean()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }

      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Boolean ) {
            return ( ( Boolean )value ).booleanValue();
         } else if ( value instanceof String ) {
            return Boolean.getBoolean( ( String )value );
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }

   }

   public byte readByte()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }

      try {
         Object value = content.get( position );
         position++;
         offset = 0;
         if ( value instanceof Byte ) {
            return ( ( Byte )value ).byteValue();
         } else if ( value instanceof String ) {
            return Byte.parseByte( ( String )value );
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public short readShort()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Byte ) {
            return ( ( Byte )value ).shortValue();
         } else if ( value instanceof Short ) {
            return ( ( Short )value ).shortValue();
         } else if ( value instanceof String ) {
            return Short.parseShort( ( String )value );
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public char readChar()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Character ) {
            return ( ( Character )value ).charValue();
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public int readInt()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }

      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Byte ) {
            return ( ( Byte )value ).intValue();
         } else if ( value instanceof Short ) {
            return ( ( Short )value ).intValue();
         } else if ( value instanceof Integer ) {
            return ( ( Integer )value ).intValue();
         } else if ( value instanceof String ) {
            return Integer.parseInt( ( String )value );
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public long readLong()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Byte ) {
            return ( ( Byte )value ).longValue();
         } else if ( value instanceof Short ) {
            return ( ( Short )value ).longValue();
         } else if ( value instanceof Integer ) {
            return ( ( Integer )value ).longValue();
         } else if ( value instanceof Long ) {
            return ( ( Long )value ).longValue();
         } else if ( value instanceof String ) {
            return Long.parseLong( ( String )value );
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public float readFloat()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Float ) {
            return ( ( Float )value ).floatValue();
         } else if ( value instanceof String ) {
            return Float.parseFloat( ( String )value );
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public double readDouble()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Float ) {
            return ( ( Float )value ).doubleValue();
         } else if ( value instanceof Double ) {
            return ( ( Double )value ).doubleValue();
         } else if ( value instanceof String ) {
            return Double.parseDouble( ( String )value );
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public String readString()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         if ( value instanceof Boolean ) {
            return ( ( Boolean )value ).toString();
         } else if ( value instanceof Byte ) {
            return ( ( Byte )value ).toString();
         } else if ( value instanceof Short ) {
            return ( ( Short )value ).toString();
         } else if ( value instanceof Character ) {
            return ( ( Character )value ).toString();
         } else if ( value instanceof Integer ) {
            return ( ( Integer )value ).toString();
         } else if ( value instanceof Long ) {
            return ( ( Long )value ).toString();
         } else if ( value instanceof Float ) {
            return ( ( Float )value ).toString();
         } else if ( value instanceof Double ) {
            return ( ( Double )value ).toString();
         } else if ( value instanceof String ) {
            return ( String )value;
         } else {
            throw new MessageFormatException( "Invalid conversion" );
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public int readBytes( byte[] value )
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object myObj = content.get( position );
         if ( !( myObj instanceof byte[] ) ) {
            throw new MessageFormatException( "Invalid conversion" );
         }
         byte[] obj = ( byte[] )myObj;

         if ( obj.length == 0 ) {
            position++;
            offset = 0;
            return 0;
         }

         if ( offset >= obj.length ) {
            return -1;
         }

         if ( obj.length - offset < value.length ) {

            for ( int i = 0; i < obj.length; i++ ) {
               value[i] = obj[i + offset];
            }

            position++;
            offset = 0;

            return obj.length - offset;
         } else {
            for ( int i = 0; i < value.length; i++ ) {
               value[i] = obj[i + offset];
            }
            offset += value.length;

            return value.length;
         }

      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public Object readObject()
      throws JMSException {
      if ( !header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is writeonly" );
      }
      try {
         Object value = content.get( position );
         position++;
         offset = 0;

         return value;
      } catch ( ArrayIndexOutOfBoundsException e ) {
         throw new MessageEOFException( "" );
      }
   }

   public void writeBoolean( boolean value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Boolean( value ) );
   }

   public void writeByte( byte value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Byte( value ) );
   }

   public void writeShort( short value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Short( value ) );
   }

   public void writeChar( char value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Character( value ) );
   }

   public void writeInt( int value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Integer( value ) );
   }

   public void writeLong( long value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Long( value ) );
   }

   public void writeFloat( float value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Float( value ) );
   }

   public void writeDouble( double value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new Double( value ) );
   }

   public void writeString( String value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( new String( value ) );
   }

   public void writeBytes( byte[] value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      content.add( value.clone() );
   }

   public void writeBytes( byte[] value, int offset, int length )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }

      if ( offset + length > value.length ) {
         throw new JMSException( "Array is too small" );
      }
      byte[] temp = new byte[length];
      for ( int i = 0; i < length; i++ ) {
         temp[i] = value[i + offset];
      }

      content.add( temp );
   }

   public void writeObject( Object value )
      throws JMSException {
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "The message body is readonly" );
      }
      if ( value instanceof Boolean ) {
         content.add( value );
      } else if ( value instanceof Byte ) {
         content.add( value );
      } else if ( value instanceof Short ) {
         content.add( value );
      } else if ( value instanceof Character ) {
         content.add( value );
      } else if ( value instanceof Integer ) {
         content.add( value );
      } else if ( value instanceof Long ) {
         content.add( value );
      } else if ( value instanceof Float ) {
         content.add( value );
      } else if ( value instanceof Double ) {
         content.add( value );
      } else if ( value instanceof String ) {
         content.add( value );
      } else if ( value instanceof byte[] ) {
         content.add( ( ( byte[] )value ).clone() );
      } else {
         throw new MessageFormatException( "Invalid object type" );
      }
   }

   public void reset()
      throws JMSException {
      header.msgReadOnly = true;
      position = 0;
      size = content.size();
      offset = 0;
   }

   public void clearBody()
      throws JMSException {
      content = new Vector();
      position = 0;
      offset = 0;
      size = 0;

      super.clearBody();
   }

   public SpyMessage myClone()
      throws JMSException {
      SpyStreamMessage result = MessagePool.getStreamMessage();
      result.copyProps( this );
      result.content = ( Vector )this.content.clone();
      result.position = this.position;
      result.offset = this.offset;
      result.size = this.size;
      return result;
   }

   public void readExternal( java.io.ObjectInput in )
      throws java.io.IOException, ClassNotFoundException {
      super.readExternal( in );
      content = ( Vector )in.readObject();
      position = in.readInt();
      offset = in.readInt();
      size = in.readInt();
   }

   public void writeExternal( java.io.ObjectOutput out )
      throws java.io.IOException {
      super.writeExternal( out );
      out.writeObject( content );
      out.writeInt( position );
      out.writeInt( offset );
      out.writeInt( size );
   }

}
