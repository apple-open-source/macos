/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Externalizable;

import java.util.Enumeration;
import java.util.Hashtable;
import javax.jms.JMSException;

import javax.jms.MapMessage;
import javax.jms.MessageFormatException;
import javax.jms.MessageNotWriteableException;

/**
 *  This class implements javax.jms.MapMessage
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.6 $
 */
public class SpyMapMessage
       extends SpyMessage
       implements MapMessage, Cloneable, Externalizable {
   // Attributes ----------------------------------------------------

   Hashtable        content;

   private final static long serialVersionUID = -4917633165373197269L;

   // Constructor ---------------------------------------------------

   public SpyMapMessage() {
      content = new Hashtable();
   }

   private void checkName( String name )
   {
      if( name == null )
         throw new IllegalArgumentException( "Name must not be null." );

      if( name.equals("") )
         throw new IllegalArgumentException( "Name must not be an empty String." );

   }

   public void setBoolean( String name, boolean value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Boolean( value ) );
   }

   public void setByte( String name, byte value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Byte( value ) );
   }

   public void setShort( String name, short value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Short( value ) );
   }

   public void setChar( String name, char value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Character( value ) );
   }

   public void setInt( String name, int value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Integer( value ) );
   }

   public void setLong( String name, long value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Long( value ) );
   }

   public void setFloat( String name, float value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Float( value ) );
   }

   public void setDouble( String name, double value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, new Double( value ) );
   }

   public void setString( String name, String value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, value );
   }

   public void setBytes( String name, byte[] value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }
      content.put( name, value.clone() );
   }

   public void setBytes( String name, byte[] value, int offset, int length )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }

      if ( offset + length > value.length ) {
         throw new JMSException( "Array is too small" );
      }
      byte[] temp = new byte[length];
      for ( int i = 0; i < length; i++ ) {
         temp[i] = value[i + offset];
      }

      content.put( name, temp );
   }

   public void setObject( String name, Object value )
      throws JMSException
   {
      checkName( name );
      if ( header.msgReadOnly ) {
         throw new MessageNotWriteableException( "Message is ReadOnly !" );
      }

      if ( value instanceof Boolean ) {
         content.put( name, value );
      } else if ( value instanceof Byte ) {
         content.put( name, value );
      } else if ( value instanceof Short ) {
         content.put( name, value );
      } else if ( value instanceof Character ) {
         content.put( name, value );
      } else if ( value instanceof Integer ) {
         content.put( name, value );
      } else if ( value instanceof Long ) {
         content.put( name, value );
      } else if ( value instanceof Float ) {
         content.put( name, value );
      } else if ( value instanceof Double ) {
         content.put( name, value );
      } else if ( value instanceof String ) {
         content.put( name, value );
      } else if ( value instanceof byte[] ) {
         content.put( name, ( ( byte[] )value ).clone() );
      } else {
         throw new MessageFormatException( "Invalid object type" );
      }
   }

   // Public --------------------------------------------------------

   public boolean getBoolean( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return Boolean.getBoolean( null );
      }

      if ( value instanceof Boolean ) {
         return ( ( Boolean )value ).booleanValue();
      } else if ( value instanceof String ) {
         return Boolean.getBoolean( ( String )value );
      } else {
         throw new MessageFormatException( "Invalid conversion" );
      }
   }

   public byte getByte( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return Byte.parseByte( null );
      }

      if ( value instanceof Byte ) {
         return ( ( Byte )value ).byteValue();
      } else if ( value instanceof String ) {
         return Byte.parseByte( ( String )value );
      } else {
         throw new MessageFormatException( "Invalid conversion" );
      }
   }

   public short getShort( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return Short.parseShort( null );
      }

      if ( value instanceof Byte ) {
         return ( ( Byte )value ).shortValue();
      } else if ( value instanceof Short ) {
         return ( ( Short )value ).shortValue();
      } else if ( value instanceof String ) {
         return Short.parseShort( ( String )value );
      } else {
         throw new MessageFormatException( "Invalid conversion" );
      }
   }

   public char getChar( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         throw new NullPointerException( "Invalid conversion" );
      }

      if ( value instanceof Character ) {
         return ( ( Character )value ).charValue();
      } else {
         throw new MessageFormatException( "Invalid conversion" );
      }
   }

   public int getInt( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return Integer.parseInt( null );
      }

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
   }

   public long getLong( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return Long.parseLong( null );
      }

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
   }

   public float getFloat( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return Float.parseFloat( null );
      }

      if ( value instanceof Float ) {
         return ( ( Float )value ).floatValue();
      } else if ( value instanceof String ) {
         return Float.parseFloat( ( String )value );
      } else {
         throw new MessageFormatException( "Invalid conversion" );
      }
   }

   public double getDouble( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return Double.parseDouble( null );
      }

      if ( value instanceof Float ) {
         return ( ( Float )value ).doubleValue();
      } else if ( value instanceof Double ) {
         return ( ( Double )value ).doubleValue();
      } else if ( value instanceof String ) {
         return Double.parseDouble( ( String )value );
      } else {
         throw new MessageFormatException( "Invalid conversion" );
      }
   }

   public String getString( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return null;
      }

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
   }

   public byte[] getBytes( String name )
      throws JMSException {
      Object value = content.get( name );
      if ( value == null ) {
         return null;
      }
      if ( value instanceof byte[] ) {
         return ( byte[] )value;
      } else {
         throw new MessageFormatException( "Invalid conversion" );
      }
   }

   public Object getObject( String name )
      throws JMSException {
      return content.get( name );
   }

   public Enumeration getMapNames()
      throws JMSException {
      return content.keys();
   }

   public boolean itemExists( String name )
      throws JMSException {
      return content.containsKey( name );
   }

   public void clearBody()
      throws JMSException {
      content = new Hashtable();
      super.clearBody();
   }

   public SpyMessage myClone()
      throws JMSException {
      SpyMapMessage result = MessagePool.getMapMessage();
      result.copyProps( this );
      result.content = ( Hashtable )this.content.clone();
      return result;
   }

   public void writeExternal( java.io.ObjectOutput out )
      throws java.io.IOException {
      super.writeExternal( out );
      out.writeObject( content );
   }

   public void readExternal( java.io.ObjectInput in )
      throws java.io.IOException, ClassNotFoundException {
      super.readExternal( in );
      content = ( Hashtable )in.readObject();
   }

}
/*
vim:ts=3:sw=3:et
*/
