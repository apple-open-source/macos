/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm;

import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;

import javax.jms.JMSException;
import org.jboss.mq.ConnectionToken;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.server.JMSDestination;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.xml.XElement;
import org.jboss.mq.server.MessageReference;
import org.jboss.mq.server.MessageCache;

/**
 *  This class allows provides the base for user supplied persistence packages.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     Paul Kendall (paul.kendall@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.7.2.1 $
 */
public interface PersistenceManager {
   
   public MessageCache getMessageCacheInstance();

   /**
    *  Create and return a unique transaction id.
    *
    * @return                             Description of the Returned Value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public Tx createPersistentTx()
      throws javax.jms.JMSException;

   /**
    *  Commit the transaction to the persistent store.
    *
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void commitPersistentTx( Tx txId )
      throws javax.jms.JMSException;

   /**
    *  Rollback the transaction.
    *
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void rollbackPersistentTx( Tx txId )
      throws javax.jms.JMSException;


   public TxManager getTxManager();


   /**
    *  Remove message from the persistent store. If the message is part of a
    *  transaction, txId is not null.
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void add( MessageReference message, Tx txId )
      throws javax.jms.JMSException;

   /**
    *  Initialize the queue.
    *
    * @param  dest                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   //public abstract void initQueue( SpyDestination dest )
   // throws javax.jms.JMSException;

   //public void initQueue( JMSDestination dest )
   //throws javax.jms.JMSException;

   public void restoreQueue(JMSDestination jmsDest, SpyDestination dest)
      throws javax.jms.JMSException;

   /**
    *  Update message in the persistent store. If the message is part of a
    *  transaction, txId is not null (not currently supported).
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void update( MessageReference message, Tx txId )
      throws javax.jms.JMSException;

   /**
    *  Remove message from the persistent store. If the message is part of a
    *  transaction, txId is not null.
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void remove( MessageReference message, Tx txId )
      throws javax.jms.JMSException;
      
   public void closeQueue(JMSDestination jmsDest, SpyDestination dest)
      throws javax.jms.JMSException;      

   /**
    * @param  server                      org.jboss.mq.server.JMSServer
    * @exception  javax.jms.JMSException  The exception description.
    */
   //void restore( org.jboss.mq.server.JMSServer server )
   //throws javax.jms.JMSException;
}
