/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import javax.jms.JMSException;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;

/**
 *  This class implements a persistent version of the basic queue.
 *
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 */

public class PersistentQueue extends org.jboss.mq.server.BasicQueue
{
   SpyDestination destination;

   public PersistentQueue(JMSDestinationManager server, SpyDestination destination, BasicQueueParameters parameters) throws JMSException
   {
      super(server, destination.toString(), parameters);
      this.destination = destination;
   }

   public SpyDestination getSpyDestination()
   {
      return destination;
   }

   public void addMessage(MessageReference mesRef, org.jboss.mq.pm.Tx txId) throws JMSException
   {

      SpyMessage mes = mesRef.getMessage();

      mes.setJMSDestination(destination);
      mesRef.invalidate();
      if (mesRef.isPersistent())
         server.getPersistenceManager().add(mesRef, txId);

      super.addMessage(mesRef, txId);
   }
}
