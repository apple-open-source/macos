/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Serializable;

import javax.jms.Destination;
import javax.naming.Referenceable;

/**
 *  This class implements javax.jms.Destination
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.5 $
 */
public class SpyDestination
       implements Destination, Serializable {
   // Attributes ----------------------------------------------------

   protected String name;
   protected int    hash;

   protected final static int NULL = 0;
   protected final static int OBJECT = 1;
   protected final static int SPY_QUEUE = 2;
   protected final static int SPY_TOPIC = 3;
   protected final static int SPY_TEMP_QUEUE = 4;
   protected final static int SPY_TEMP_TOPIC = 5;

   SpyDestination( String name ) {
      this.name = name;
      hash = name.hashCode();
   }

   /**
    *  gets the name of the destination.
    *
    * @return    java.lang.String
    */
   public java.lang.String getName() {
      return name;
   }

   public int hashCode() {
      return hash;
   }

   public static void writeDest( java.io.ObjectOutput out, javax.jms.Destination dest )
      throws java.io.IOException {
      if ( dest == null ) {
         out.writeByte( NULL );
      } else if ( dest instanceof SpyTemporaryQueue ) {
         out.writeByte( SPY_TEMP_QUEUE );
         out.writeUTF( ( ( SpyTemporaryQueue )dest ).getName() );
      } else if ( dest instanceof SpyTemporaryTopic ) {
         out.writeByte( SPY_TEMP_TOPIC );
         out.writeUTF( ( ( SpyTemporaryTopic )dest ).getName() );
      } else if ( dest instanceof SpyQueue ) {
         out.writeByte( SPY_QUEUE );
         out.writeUTF( ( ( SpyQueue )dest ).getName() );
      } else if ( dest instanceof SpyTopic ) {
         out.writeByte( SPY_TOPIC );
         out.writeUTF( ( ( SpyTopic )dest ).getName() );
         DurableSubscriptionID id = ( ( SpyTopic )dest ).durableSubscriptionID;
         if ( id == null ) {
            out.writeByte( NULL );
         } else {
            out.writeByte( OBJECT );
            writeString( out, id.getClientID() );
            writeString( out, id.getSubscriptionName() );
            writeString( out, id.getSelector() );
         }
      } else {
         out.writeByte( OBJECT );
         out.writeObject( dest );
      }
   }

   public static javax.jms.Destination readDest( java.io.ObjectInput in )
      throws java.io.IOException {
      byte destType = in.readByte();
      if ( destType == NULL ) {
         return null;
      } else if ( destType == SPY_TEMP_QUEUE ) {
         return new SpyTemporaryQueue( in.readUTF(), null );
      } else if ( destType == SPY_TEMP_TOPIC ) {
         return new SpyTemporaryTopic( in.readUTF(), null );
      } else if ( destType == SPY_QUEUE ) {
         return new SpyQueue( in.readUTF() );
      } else if ( destType == SPY_TOPIC ) {
         String name = in.readUTF();
         destType = in.readByte();
         if ( destType == NULL ) {
            return new SpyTopic( name );
         } else {
            String clientId = readString( in );
            String subName = readString( in );
            String selector = readString( in );
            return new SpyTopic( new SpyTopic( name ), clientId, subName, selector );
         }
      } else {
         try {
            return ( Destination )in.readObject();
         } catch ( ClassNotFoundException e ) {
            throw new java.io.IOException( "Class not found for unknown destination." );
         }
      }
   }

   private static void writeString( java.io.ObjectOutput out, String s )
      throws java.io.IOException {
      if ( s == null ) {
         out.writeByte( NULL );
      } else {
         out.writeByte( OBJECT );
         //non-null
         out.writeUTF( s );
      }
   }

   private static String readString( java.io.ObjectInput in )
      throws java.io.IOException {
      byte b = in.readByte();
      if ( b == NULL ) {
         return null;
      } else {
         return in.readUTF();
      }
   }

}
