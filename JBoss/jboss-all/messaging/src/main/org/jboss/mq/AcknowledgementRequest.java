/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Serializable;
import javax.jms.Destination;

/**
 *  Used to Acknowledge sent messages. This class holds the minimum abount of
 *  information needed to identify a message to the JMSServer.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class AcknowledgementRequest
       implements java.io.Serializable, java.io.Externalizable {

   public boolean   isAck;
   public Destination destination = null;
   public String    messageID = null;
   public int       subscriberId;

   public boolean equals( Object o ) {

      if ( !( o instanceof AcknowledgementRequest ) ) {
         return false;
      }

      return messageID.equals( ( ( AcknowledgementRequest )o ).messageID ) &&
            destination.equals( ( ( AcknowledgementRequest )o ).destination ) &&
            subscriberId == ( ( AcknowledgementRequest )o ).subscriberId;
   }

   public int hashCode() {
      return messageID.hashCode();
   }

   public String toString() {
      return "AcknowledgementRequest:" + ( isAck ? "ACK" : "NACK" ) + "," + destination + "," + messageID;
   }

   public void readExternal( java.io.ObjectInput in )
      throws java.io.IOException {
      isAck = in.readBoolean();
      destination = SpyDestination.readDest( in );
      messageID = in.readUTF();
      subscriberId = in.readInt();
   }

   public void writeExternal( java.io.ObjectOutput out )
      throws java.io.IOException {
      out.writeBoolean( isAck );
      SpyDestination.writeDest( out, destination );
      out.writeUTF( messageID );
      out.writeInt( subscriberId );
   }

}
