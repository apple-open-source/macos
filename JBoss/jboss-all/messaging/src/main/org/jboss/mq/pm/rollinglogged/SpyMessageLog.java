/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.rollinglogged;


import java.io.File;
import java.io.IOException;
import java.io.Serializable;
import java.util.HashMap;
import javax.jms.JMSException;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.server.MessageReference;
import org.jboss.mq.server.MessageCache;

import org.jboss.mq.SpyMessage;

/**
 *  This is used to keep a log of SpyMessages arriving and leaving a queue. The
 *  log can be used reconstruct the queue in case of provider failure. Integrety
 *  is kept by the use of an ObjectIntegrityLog.
 *
 * @author:    Hiram Chirino (Cojonudo14@hotmail.com)
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version    $Revision: 1.7.2.1 $
 */
public class SpyMessageLog {

   /////////////////////////////////////////////////////////////////////
   // Attributes
   /////////////////////////////////////////////////////////////////////
   private IntegrityLog transactionLog;

   private final MessageCache cache;

   /////////////////////////////////////////////////////////////////////
   // Constructor
   /////////////////////////////////////////////////////////////////////
   SpyMessageLog(MessageCache cache, File file )
      throws JMSException {
      if (cache == null) 
      {
         throw new IllegalArgumentException("must supply a cache!");
      } // end of if ()
      
      this.cache = cache;
      try {
         transactionLog = new IntegrityLog( file );
      } catch ( IOException e ) {
         throwJMSException( "Could not open the queue's tranaction log: " + file.getAbsolutePath(), e );
      }
   }


   /////////////////////////////////////////////////////////////////////
   // Public Methods
   /////////////////////////////////////////////////////////////////////
   public synchronized void close()
      throws JMSException {
      try {
         transactionLog.close();
      } catch ( IOException e ) {
         throwJMSException( "Could not close the queue's tranaction log.", e );
      }
   }

   public synchronized void delete()
      throws JMSException {
      try {
         transactionLog.delete();
      } catch ( IOException e ) {
         throwJMSException( "Could not delete the queue's tranaction log.", e );
      }
   }


   public synchronized void restore( java.util.TreeSet committed, PersistenceManager.LogInfo info, HashMap messages)
      throws JMSException {

      //<<<<<<< SpyMessageLog.java
      //java.util.HashMap messageIndex = new java.util.HashMap();
           //info.liveMessages = 0;
      //=======
      //java.util.HashMap messageIndex = new java.util.HashMap();
      
      //>>>>>>> 1.5
      try {
         java.util.LinkedList objects = transactionLog.toIndex();

         for ( java.util.Iterator it = objects.iterator(); it.hasNext();  ) {

            Object o = it.next();
            if ( o instanceof IntegrityLog.MessageAddedRecord ) {

               IntegrityLog.MessageAddedRecord r = ( IntegrityLog.MessageAddedRecord )o;
               r.message.header.messageId = r.messageId;

               if ( r.isTransacted && !committed.contains( new org.jboss.mq.pm.Tx( r.transactionId ) ) ) {
                  // the TX this message was part of was not
                  // committed... so drop this message
                  continue;
               }
               MessageReference mr = cache.add(r.message);
               mr.persistData = info;
               messages.put(new Long(r.messageId), mr);
               info.liveMessages++;


            } else if ( o instanceof IntegrityLog.MessageRemovedRecord ) {

               IntegrityLog.MessageRemovedRecord r = ( IntegrityLog.MessageRemovedRecord )o;

               if ( r.isTransacted && !committed.contains( new org.jboss.mq.pm.Tx( r.transactionId ) ) ) {
                  // the TX this message read was part of was not
                  // committed... so keep this message
                  continue;
               }
               //<<<<<<< SpyMessageLog.java
               MessageReference mr = (MessageReference)messages.remove(new Long(r.messageId));                  
               if( mr != null )
                  cache.remove(mr);   
               //messageIndex.remove( new Long( r.messageId ) );
               info.liveMessages--;
               /*=======

               Long txid = new Long( r.messageId );	
               MessageReference mr = (MessageReference)messageIndex.get(txid );
               messageIndex.remove(txid );
               if( mr != null )
                  cache.remove(mr);   
               */
               //>>>>>>> 1.5
            }
            else if ( o instanceof IntegrityLog.MessageUpdateRecord )
            {

               IntegrityLog.MessageUpdateRecord r = ( IntegrityLog.MessageUpdateRecord )o;
               r.message.header.messageId = r.messageId;

               if ( r.isTransacted && !committed.contains( new org.jboss.mq.pm.Tx( r.transactionId ) ) ) {
                  // the TX this message was part of was not
                  // committed... so drop this message
                  continue;
               }

               // Invalidate any cached disk copy and change the message
               MessageReference mr = (MessageReference) messages.get(new Long(r.messageId));
               if (mr != null)
               {
                  mr.invalidate();
                  mr.hardReference = r.message;
               }
            }
         }
      } catch ( Exception e ) {
//      e.printStackTrace();
         throwJMSException( "Could not rebuild the queue from the queue's tranaction log.", e );
      }

//<<<<<<< SpyMessageLog.java
      /*
      SpyMessage rc[] = new SpyMessage[messageIndex.size()];
=======
      MessageReference rc[] = new MessageReference[messageIndex.size()];
>>>>>>> 1.5
      java.util.Iterator iter = messageIndex.values().iterator();
      for ( int i = 0; iter.hasNext(); i++ ) {
         rc[i] = (MessageReference)iter.next();
      }
      return rc;*/
   }

   public synchronized void add( SpyMessage message, org.jboss.mq.pm.Tx transactionId )
      throws JMSException {
      try {

         if ( transactionId == null ) {
            transactionLog.add( message.header.messageId, false, -1, message );
         } else {
            transactionLog.add( message.header.messageId, true, transactionId.longValue(), message );
         }
         transactionLog.commit();

      } catch ( IOException e ) {
         throwJMSException( "Could not write to the tranaction log.", e );
      }

   }

   public synchronized void remove( SpyMessage message, org.jboss.mq.pm.Tx transactionId )
      throws JMSException {
      try {

         if ( transactionId == null ) {
            transactionLog.remove( message.header.messageId, false, -1 );
         } else {
            transactionLog.remove( message.header.messageId, true, transactionId.longValue() );
         }
         transactionLog.commit();

      } catch ( IOException e ) {
         throwJMSException( "Could not write to the queue's tranaction log.", e );
      }

   }

   public synchronized void update( SpyMessage message, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
      try
      {
         if ( transactionId == null )
         {
            transactionLog.update( message.header.messageId, false, -1, message );
         }
         else
         {
            throw new JMSException("NYI: No code does updates in a transaction");
         }
         transactionLog.commit();

      }
      catch ( IOException e )
      {
         throwJMSException( "Could not write to the queue's tranaction log.", e );
      }

   }

   private void throwJMSException( String message, Exception e )
      throws JMSException {
      JMSException newE = new SpyJMSException( message );
      newE.setLinkedException( e );
      throw newE;
   }
}
