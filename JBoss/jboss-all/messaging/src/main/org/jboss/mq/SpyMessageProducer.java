/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import javax.jms.DeliveryMode;
import javax.jms.JMSException;
import javax.jms.Message;

import javax.jms.MessageProducer;

/**
 *  This class implements javax.jms.MessageProducer
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class SpyMessageProducer
       implements MessageProducer {
   protected int    defaultDeliveryMode = SpyMessage.DEFAULT_DELIVERY_MODE;
   protected int    defaultPriority = SpyMessage.DEFAULT_PRIORITY;
   protected long   defaultTTL = SpyMessage.DEFAULT_TIME_TO_LIVE;
   // Attributes ----------------------------------------------------

   private boolean  disableMessageID = false;
   private boolean  disableTS = false;

   // Public --------------------------------------------------------

   public void setDisableMessageID( boolean value )
      throws JMSException {
      disableMessageID = value;
   }

   public void setDisableMessageTimestamp( boolean value )
      throws JMSException {
      disableTS = value;
   }

   public void setDeliveryMode( int deli )
      throws JMSException {
      if ( deli != DeliveryMode.NON_PERSISTENT && deli != DeliveryMode.PERSISTENT ) {
         throw new JMSException( "Bad DeliveryMode value" );
      } else {
         defaultDeliveryMode = deli;
      }
   }

   public void setPriority( int pri )
      throws JMSException {
      if ( pri < 0 || pri > 9 ) {
         throw new JMSException( "Bad priority value" );
      } else {
         defaultPriority = pri;
      }
   }

   public void setTimeToLive( int timeToLive )
      throws JMSException {
      if ( timeToLive < 0 ) {
         throw new JMSException( "Bad TimeToLive value" );
      } else {
         defaultTTL = timeToLive;
      }
   }

   public void setTimeToLive( long timeToLive )
      throws JMSException {
      if ( timeToLive < 0 ) {
         throw new JMSException( "Bad TimeToLive value" );
      } else {
         defaultTTL = timeToLive;
      }
   }

   public boolean getDisableMessageID()
      throws JMSException {
      return disableMessageID;
   }

   public boolean getDisableMessageTimestamp()
      throws JMSException {
      return disableTS;
   }

   public int getDeliveryMode()
      throws JMSException {
      return defaultDeliveryMode;
   }

   public int getPriority()
      throws JMSException {
      return defaultPriority;
   }

   public long getTimeToLive()
      throws JMSException {
      return defaultTTL;
   }


   public void close()
      throws JMSException {
      //Is there anything useful to do ?
      //Let the GC do its work !
   }
}
