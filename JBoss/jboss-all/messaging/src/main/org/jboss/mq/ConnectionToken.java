/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import java.io.Serializable;

import java.util.HashMap;

import org.jboss.mq.il.ClientIL;

/**
 *  This class is the broker point of view on a SpyConnection (it contains a
 *  ConnectionReceiver).
 *
 * Remember that for most IL's it will be serialized!
 *
 * @author    <a href="Norbert.Lataille@m4x.org">Norbert Lataille</a>
 * @author    <a href="Cojonudo14@hotmail.com">Hiram Chirino</a>
 * @author    <a href="pra@tim.se">Peter Antman</a>
 * @created    August 16, 2001
 * @version    $Revision: 1.3 $
 */
public class ConnectionToken
       implements Serializable {

   //public transient ConnectionReceiverSetup cr_server;
   /**
    * Used by the server to callback to client. Will (most of the time)
    * be serialized when sent to the server.
    */
   public ClientIL  clientIL;

   /**
    * The clientID of the connection.
    */
   protected String   clientID;
   
   /**
    * A secured hashed unique sessionId that is valid only as long
    * as the connection live. Set during authentication and used 
    * for autorization.
    */
   private String sessionId;
   private int      hash;

   public ConnectionToken( String clientID, ClientIL clientIL ) {
      this.clientIL = clientIL;
      setClientID(clientID);
   }
   public ConnectionToken( String clientID, ClientIL clientIL,String sessionId ) {
      this(clientID, clientIL);
      this.sessionId = sessionId;
   }
   
   public String getClientID() {
      return clientID;
   }

   public void setClientID(String clientID) {
      this.clientID = clientID;
      if ( clientID == null ) {
         hash = 0;
      } else {
         hash = clientID.hashCode();
      }
   }

   public String getSessionId() {
      return sessionId;
   }

   public boolean equals( Object obj ) {
      // Fixes NPE. Patch submitted by John Ellis (10/29/00)
      if(!(obj instanceof ConnectionToken) || obj == null)
         return false;
      
      if ( obj.hashCode() != hash ) {
         return false;
      }
      String yourID =  (( ConnectionToken )obj ).clientID;
      if (clientID != null)
         return clientID.equals(yourID);
      else if (yourID == null)
         return true;//We know clientID is null
      else
         return false;//clientID is null, yourID is not.
   }

   public int hashCode() {
      return hash;
   }


   public String toString() {
      return "SpyDistributedConnection:" + clientID+"/"+sessionId;
   }
}
