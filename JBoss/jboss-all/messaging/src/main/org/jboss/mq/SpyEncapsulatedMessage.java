/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import javax.jms.Message;

/**
 *  This Message class is used to send a non 'provider-optimized Message' over
 *  the network [4.4.5]
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.4 $
 */
public class SpyEncapsulatedMessage
       extends SpyObjectMessage {

   private final static long serialVersionUID = 3995327252678969050L;

   public void setMessage( Message m )
      throws javax.jms.JMSException {
      this.setObject( ( java.io.Serializable )m );

      setJMSCorrelationID( m.getJMSCorrelationID() );
      setJMSCorrelationIDAsBytes( m.getJMSCorrelationIDAsBytes() );
      setJMSReplyTo( m.getJMSReplyTo() );
      setJMSType( m.getJMSType() );
      setJMSDestination( m.getJMSDestination() );
      setJMSDeliveryMode( m.getJMSDeliveryMode() );
      setJMSExpiration( m.getJMSExpiration() );
      setJMSPriority( m.getJMSPriority() );
      setJMSMessageID( m.getJMSMessageID() );
      setJMSTimestamp( m.getJMSTimestamp() );

      java.util.Enumeration enum = m.getPropertyNames();
      while ( enum.hasMoreElements() ) {
         String name = ( String )enum.nextElement();
         Object o = m.getObjectProperty( name );
         setObjectProperty( name, o );
      }
   }

   public Message getMessage()
      throws javax.jms.JMSException {
      Message m = ( Message )this.getObject();
      m.setJMSRedelivered( getJMSRedelivered() );
      return m;
   }

   public SpyMessage myClone()
      throws javax.jms.JMSException {
      SpyEncapsulatedMessage result = MessagePool.getEncapsulatedMessage();
      result.copyProps( this );
      //HACK to get around read only problem
      boolean readOnly = result.header.msgReadOnly;
      result.header.msgReadOnly = false;
      result.setMessage( this.getMessage() );
      result.header.msgReadOnly = readOnly;
      return result;
   }
}
