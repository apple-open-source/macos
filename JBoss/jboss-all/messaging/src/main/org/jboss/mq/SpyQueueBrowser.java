/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.util.Enumeration;
import java.util.NoSuchElementException;

import javax.jms.InvalidSelectorException;
import javax.jms.JMSException;
import javax.jms.Queue;
import org.jboss.mq.selectors.Selector;

import javax.jms.QueueBrowser;

/**
 *  This class implements javax.jms.QueueBrowser
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.3.2.1 $
 */
public class SpyQueueBrowser
       implements QueueBrowser {

   boolean          closed;
   // The destination this browser will browse messages from
   Queue            destination;
   // String Selector
   String           messageSelector;
   // The QueueSession this was created with
   SpyQueueSession  session;

   SpyQueueBrowser( SpyQueueSession session, Queue destination, String messageSelector ) 
   	throws InvalidSelectorException {
      this.destination = destination;
      this.session = session;
      this.messageSelector = messageSelector;
      
      // If the selector is set, try to build it, throws an InvalidSelectorException 
      // if it is not valid.
      if( messageSelector!=null )
      	new Selector(messageSelector);
   }

   public Queue getQueue()
      throws JMSException {
      return destination;
   }

   public String getMessageSelector()
      throws JMSException {
      return messageSelector;
   }

   public Enumeration getEnumeration()
      throws JMSException {
      if ( closed ) {
         throw new JMSException( "The QueueBrowser was closed" );
      }

      final SpyMessage data[] = session.connection.browse( destination, messageSelector );
      return new Enumeration() {
         int i = 0;
         public boolean hasMoreElements() {
            return i < data.length;
         }
         public Object nextElement() {
            if (!hasMoreElements())
               throw new NoSuchElementException();
            return data[i++];
      }
      };
   }

   public void close()
      throws JMSException {
      closed = true;
      return;
   }
}
