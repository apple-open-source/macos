/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.rmi;

import javax.jms.Destination;
import javax.jms.IllegalStateException;
import javax.jms.JMSException;
import org.jboss.mq.Connection;
import org.jboss.mq.ReceiveRequest;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;

/**
 * The RMI implementation of the ConnectionReceiver object
 *
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.5 $
 * @created   August 16, 2001
 */
public class RMIClientIL extends java.rmi.server.UnicastRemoteObject implements RMIClientILRemote
{

   // A reference to the connection
   Connection connection;
   // Are we running
   boolean stopped = true;

   public RMIClientIL(Connection c)
          throws java.rmi.RemoteException
   {
      connection = c;
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   public void close()
          throws Exception
   {
      if (stopped)
      {
         throw new IllegalStateException("The client IL is stopped");
      }
      connection.asynchClose();
   }

   //One TemporaryDestination has been deleted
   /**
    * #Description of the Method
    *
    * @param dest              Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void deleteTemporaryDestination(SpyDestination dest)
          throws JMSException
   {
      if (stopped)
      {
         throw new IllegalStateException("The client IL is stopped");
      }
      connection.asynchDeleteTemporaryDestination(dest);
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
      if (stopped)
      {
         throw new IllegalStateException("The client IL is stopped");
      }
      connection.asynchDeliver(messages);
   }

   /**
    * pong method comment.
    *
    * @param serverTime        Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void pong(long serverTime)
          throws JMSException
   {
      if (stopped)
      {
         throw new IllegalStateException("The client IL is stopped");
      }
      connection.asynchPong(serverTime);
   }
}
