/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil2;

import java.io.IOException;

import org.jboss.logging.Logger;
import org.jboss.mq.ReceiveRequest;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;

import EDU.oswego.cs.dl.util.concurrent.Slot;

/**
 * The OIL2 implementation of the ClientIL object
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision:$
 */
public final class OIL2ClientIL
   implements ClientIL,
      java.io.Serializable
{
   private final static Logger log = Logger.getLogger(OIL2ClientIL.class);
   
   transient OIL2ServerILService.RequestListner requestListner;
   transient OIL2SocketHandler socketHandler;

   public void setRequestListner(OIL2ServerILService.RequestListner requestListner)
   {
      this.requestListner = requestListner;
      this.socketHandler = requestListner.getSocketHandler();
   }
   
   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   public void close()
          throws Exception
   {
      try {
         
         OIL2Request request = new OIL2Request(
            OIL2Constants.CLIENT_CLOSE,
            null);
         OIL2Response response = socketHandler.synchRequest(request);
         response.evalThrowsException();
         
      } catch ( IOException ignore ) {
         // Server closes the socket before we get a response..
         // this is ok.
      }
      
      // The close request now went full cycle, from the client
      // to the server, and back from the server to the client.
      // Close up the requestListner
      // This will shut down the sockets and threads.
      requestListner.close();
   }

   /**
    * #Description of the Method
    *
    * @param dest           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void deleteTemporaryDestination(SpyDestination dest)
          throws Exception
   {
      
      OIL2Request request = new OIL2Request(
         OIL2Constants.CLIENT_DELETE_TEMPORARY_DESTINATION,
         new Object[] {dest});
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsException();
   }

   /**
    * #Description of the Method
    *
    * @param serverTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void pong(long serverTime)
          throws Exception
   {
      OIL2Request request = new OIL2Request(
         OIL2Constants.CLIENT_PONG,
         new Object[] {new Long(serverTime)});
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsException();      
   }

   /**
    * #Description of the Method
    *
    * @param messages       Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void receive(ReceiveRequest messages[])
          throws Exception
   {
      
      OIL2Request request = new OIL2Request(
         OIL2Constants.CLIENT_RECEIVE,
         new Object[] {messages});
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsException();            
   }
   
}
// vim:expandtab:tabstop=3:shiftwidth=3
