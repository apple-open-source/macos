/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;

import java.io.Serializable;

/**
 *  This class contians all the data needed to perform a JMS transaction
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class ReceiveRequest
       implements Serializable, Externalizable {
   // The message
   public SpyMessage message;
   // Is this an exlusive message? Then subscriptionId != null
   public Integer   subscriptionId;

   protected final static byte NULL = 0;
   protected final static byte NON_NULL = 1;

   public void readExternal( ObjectInput in )
      throws IOException {
      message = SpyMessage.readMessage( in );
      byte b = in.readByte();
      if ( b == NON_NULL ) {
         subscriptionId = new Integer( in.readInt() );
      } else {
         subscriptionId = null;
      }
   }

   public void writeExternal( ObjectOutput out )
      throws IOException {
      SpyMessage.writeMessage( message, out );
      if ( subscriptionId == null ) {
         out.writeByte( NULL );
      } else {
         out.writeByte( NON_NULL );
         out.writeInt( subscriptionId.intValue() );
      }
   }

}
