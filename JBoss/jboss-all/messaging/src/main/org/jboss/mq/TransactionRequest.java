/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

/**
 *  This class contians all the data needed to perform a JMS transaction
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class TransactionRequest
       implements java.io.Serializable, java.io.Externalizable {

   //Request type
   public byte      requestType = ONE_PHASE_COMMIT_REQUEST;

   //For 2 phase commit, this identifies the transaction.
   public Object    xid;

   // messages sent in the transaction
   public SpyMessage[] messages;

   // messages acknowleged in the transaction
   public AcknowledgementRequest[] acks;
   //Valid requests types
   public final static byte ONE_PHASE_COMMIT_REQUEST = 0;
   public final static byte TWO_PHASE_COMMIT_COMMIT_REQUEST = 2;
   public final static byte TWO_PHASE_COMMIT_PREPARE_REQUEST = 1;
   public final static byte TWO_PHASE_COMMIT_ROLLBACK_REQUEST = 3;

   public void readExternal( java.io.ObjectInput in )
      throws java.io.IOException {
      requestType = in.readByte();
      try {
         xid = in.readObject();
      } catch ( ClassNotFoundException e ) {
         throw new java.io.IOException( "Class not found for xid." );
      }
      int size = in.readInt();
      messages = new SpyMessage[size];
      for ( int i = 0; i < size; ++i ) {
         messages[i] = SpyMessage.readMessage( in );
      }
      size = in.readInt();
      acks = new AcknowledgementRequest[size];
      for ( int i = 0; i < size; ++i ) {
         acks[i] = new AcknowledgementRequest();
         acks[i].readExternal( in );
      }
   }

   public void writeExternal( java.io.ObjectOutput out )
      throws java.io.IOException {
      out.writeByte( requestType );
      out.writeObject( xid );
      if ( messages == null ) {
         out.writeInt( 0 );
      } else {
         out.writeInt( messages.length );
         for ( int i = 0; i < messages.length; ++i ) {
            SpyMessage.writeMessage( messages[i], out );
         }
      }
      if ( acks == null ) {
         out.writeInt( 0 );
      } else {
         out.writeInt( acks.length );
         for ( int i = 0; i < acks.length; ++i ) {
            acks[i].writeExternal( out );
         }
      }
   }

}
