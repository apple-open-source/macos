/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il;

import org.jboss.mq.ReceiveRequest;
import org.jboss.mq.SpyDestination;

/**
 * This interface defines the methods that the server can make asynchronouly to
 * a client. (ie. to deliver messages)
 *
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @version   $Revision: 1.4 $
 * @created   August 16, 2001
 */
public interface ClientIL
{

   /**
    * One TemporaryDestination has been deleted
    *
    * @param dest           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void deleteTemporaryDestination(SpyDestination dest)
          throws Exception;

   /**
    * The connection is closed
    *
    * @exception Exception  Description of Exception
    */
   public void close()
          throws Exception;

   //
   /**
    * A message has arrived for the Connection. Deliver messages to client.
    *
    * @param messages       Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void receive(ReceiveRequest messages[])
          throws Exception;

   /**
    *  Response to a ping sent by a client.
    *
    * @param serverTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void pong(long serverTime) throws Exception;
}
