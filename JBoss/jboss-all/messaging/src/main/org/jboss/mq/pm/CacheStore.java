/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm;

import javax.jms.JMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.server.MessageReference;

/**
 *  This class provides the base for user supplied persistence packages.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     Paul Kendall (paul.kendall@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.1 $
 */
public interface CacheStore {

   /**
    * reads the message refered to by the MessagReference back as a SpyMessage
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   SpyMessage loadFromStorage(MessageReference mh) throws JMSException;

   /**
    * Stores the given message to secondary storeage.  You should be able to
    * use the MessagReference to load the message back later.
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   void saveToStorage(MessageReference mh, SpyMessage message) throws JMSException;

   /**
    * Removes the message that was stored in secondary storage.
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   void removeFromStorage(MessageReference mh) throws JMSException;

}