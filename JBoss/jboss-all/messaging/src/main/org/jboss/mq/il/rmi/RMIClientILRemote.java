/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.rmi;

import javax.jms.Destination;
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
 * @version   $Revision: 1.3 $
 * @created   August 16, 2001
 */
public interface RMIClientILRemote extends ClientIL, java.rmi.Remote
{

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   public void close()
          throws Exception;

   //One TemporaryDestination has been deleted
   /**
    * #Description of the Method
    *
    * @param dest           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void deleteTemporaryDestination(SpyDestination dest)
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param messages       Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void receive(ReceiveRequest messages[])
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param serverTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void pong(long serverTime)
          throws Exception;
}
