/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;

import javax.jms.JMSException;

import javax.transaction.xa.XAException;
import javax.transaction.xa.Xid;

import org.jboss.logging.Logger;

import org.jboss.logging.Logger;

/**
 *  This class implements the ResourceManager used for the XAResources used int
 *  JBossMQ.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.8 $
 */
public class SpyXAResourceManager
   implements java.io.Serializable
{
   private static final Logger log = Logger.getLogger(SpyXAResourceManager.class);

   //////////////////////////////////////////////////////////////////
   // Attributes
   //////////////////////////////////////////////////////////////////
   private Connection connection;
   private Map transactions = java.util.Collections.synchronizedMap( new HashMap() );
   private long nextInternalXid = Long.MIN_VALUE;

   //Valid tx states:
   private final static byte TX_OPEN = 0;
   private final static byte TX_SUSPENDED = 2;
   private final static byte TX_PREPARED = 3;
   private final static byte TX_COMMITED = 4;
   private final static byte TX_ROLLEDBACK = 5;

   private final static byte TX_ENDED = 1;

   //////////////////////////////////////////////////////////////////
   // Constructors
   //////////////////////////////////////////////////////////////////

   public SpyXAResourceManager( Connection conn ) {
      super();
      connection = conn;
   }


   //////////////////////////////////////////////////////////////////
   // Public Methods
   //////////////////////////////////////////////////////////////////

   public void ackMessage( Object xid, SpyMessage msg )
      throws JMSException
   {
      if (log.isTraceEnabled()) {
         log.trace("Ack'ing message xid=" + xid);
      }
      
      TXState state = ( TXState )transactions.get( xid );
      if ( state == null ) {
         throw new JMSException( "Invalid transaction id." );
      }
      AcknowledgementRequest item = msg.getAcknowledgementRequest(true);
      state.ackedMessages.addLast( item );
   }

   public void addMessage( Object xid, SpyMessage msg )
      throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace) {
         log.trace("Adding xid=" + xid + ", message=" + msg);
      }
      
      TXState state = ( TXState )transactions.get( xid );
      if (trace) {
         log.trace("TXState=" + state);
      }

      if ( state == null ) {
         throw new JMSException( "Invalid transaction id." );
      }
      
      state.sentMessages.addLast( msg );
   }

   public void commit( Object xid, boolean onePhase ) 
      throws XAException, JMSException
   {
      if (log.isTraceEnabled()) {
         log.trace("Commiting xid=" + xid + ", onePhase=" + onePhase);
      }
      
      TXState state = ( TXState )transactions.remove( xid );
      if ( state == null ) {
         throw new XAException( XAException.XAER_NOTA );
      }

      if ( onePhase ) {
         TransactionRequest transaction = new TransactionRequest();
         transaction.requestType = transaction.ONE_PHASE_COMMIT_REQUEST;
         transaction.xid = null;
         if ( state.sentMessages.size() != 0 ) {
            SpyMessage job[] = new SpyMessage[state.sentMessages.size()];
            job = ( SpyMessage[] )state.sentMessages.toArray( job );
            transaction.messages = job;
         }
         if ( state.ackedMessages.size() != 0 ) {
            AcknowledgementRequest job[] = new AcknowledgementRequest[state.ackedMessages.size()];
            job = ( AcknowledgementRequest[] )state.ackedMessages.toArray( job );
            transaction.acks = job;
         }
         connection.send( transaction );
      } else {
         if ( state.txState != TX_PREPARED ) {
            throw new XAException( "The transaction had not been prepared" );
         }
         TransactionRequest transaction = new TransactionRequest();
         transaction.xid = xid;
         transaction.requestType = transaction.TWO_PHASE_COMMIT_COMMIT_REQUEST;
         connection.send( transaction );
      }
      state.txState = TX_COMMITED;
   }

   public void endTx( Object xid, boolean success )
      throws XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Ending xid=" + xid + ", success=" + success);
      }
      
      TXState state = ( TXState )transactions.get( xid );
      if ( state == null ) {
         throw new XAException( XAException.XAER_NOTA );
      }
      state.txState = TX_ENDED;
   }

   public Object joinTx( Xid xid )
      throws XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Joining tx xid=" + xid);
      }
      
      if ( !transactions.containsKey( xid ) ) {
         throw new XAException( XAException.XAER_NOTA );
      }
      return xid;
   }

   public int prepare( Object xid )
      throws XAException, JMSException
   {
      if (log.isTraceEnabled()) {
         log.trace("Preparing xid=" + xid);
      }
      
      TXState state = ( TXState )transactions.get( xid );
      if ( state == null ) {
         throw new XAException( XAException.XAER_NOTA );
      }
      TransactionRequest transaction = new TransactionRequest();
      transaction.requestType = transaction.TWO_PHASE_COMMIT_PREPARE_REQUEST;
      transaction.xid = xid;
      if ( state.sentMessages.size() != 0 ) {
         SpyMessage job[] = new SpyMessage[state.sentMessages.size()];
         job = ( SpyMessage[] )state.sentMessages.toArray( job );
         transaction.messages = job;
      }
      if ( state.ackedMessages.size() != 0 ) {
         AcknowledgementRequest job[] = new AcknowledgementRequest[state.ackedMessages.size()];
         job = ( AcknowledgementRequest[] )state.ackedMessages.toArray( job );
         transaction.acks = job;
      }
      connection.send( transaction );
      state.txState = TX_PREPARED;
      return javax.transaction.xa.XAResource.XA_OK;
   }

   public Object resumeTx( Xid xid )
      throws XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Resuming tx xid=" + xid);
      }
      
      if ( !transactions.containsKey( xid ) ) {
         throw new XAException( XAException.XAER_NOTA );
      }
      return xid;
   }

   public void rollback( Object xid )
      throws XAException, JMSException
   {
      if (log.isTraceEnabled()) {
         log.trace("Rolling back xid=" + xid);
      }

      TXState state = ( TXState )transactions.remove( xid );
      if ( state == null ) {
         throw new XAException( XAException.XAER_NOTA );
      }
      if ( state.txState != TX_PREPARED ) {
         TransactionRequest transaction = new TransactionRequest();
         transaction.requestType = transaction.ONE_PHASE_COMMIT_REQUEST;
         transaction.xid = null;
         if ( state.ackedMessages.size() != 0 ) {
            AcknowledgementRequest job[] = new AcknowledgementRequest[state.ackedMessages.size()];
            job = ( AcknowledgementRequest[] )state.ackedMessages.toArray( job );
            transaction.acks = job;
            //Neg Acknowlege all consumed messages
            for ( int i = 0; i < transaction.acks.length; i++ ) {
               transaction.acks[i].isAck = false;
            }
         }
         connection.send( transaction );
      } else {
         TransactionRequest transaction = new TransactionRequest();
         transaction.xid = xid;
         transaction.requestType = transaction.TWO_PHASE_COMMIT_ROLLBACK_REQUEST;
         connection.send( transaction );
      }
      state.txState = TX_ROLLEDBACK;
   }

   public synchronized Object startTx()
   {
      Long newXid = new Long( nextInternalXid++ );
      transactions.put( newXid, new TXState() );

      if (log.isTraceEnabled()) {
         log.trace("Starting tx with new xid=" + newXid);
      }
      
      return newXid;
   }

   public Object startTx( Xid xid )
      throws XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Starting tx xid=" + xid);
      }
      
      if ( transactions.containsKey( xid ) ) {
         throw new XAException( XAException.XAER_DUPID );
      }
      transactions.put( xid, new TXState() );
      return xid;
   }

   public Object suspendTx( Xid xid )
      throws XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Suppending tx xid=" + xid);
      }

      if ( !transactions.containsKey( xid ) ) {
         throw new XAException( XAException.XAER_NOTA );
      }
      return xid;
   }

   public Object convertTx( Long anonXid, Xid xid ) throws XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Converting tx anonXid=" + anonXid + ", xid=" + xid);
      }

      if ( !transactions.containsKey( anonXid ) ) {
         throw new XAException( XAException.XAER_NOTA );
      }
      if ( transactions.containsKey( xid ) ) {
         throw new XAException( XAException.XAER_DUPID );
      }
      TXState s = (TXState)transactions.remove( anonXid );

      transactions.put( xid,  s );
      return xid;
   }

   //////////////////////////////////////////////////////////////////
   // Helper Inner classes
   //////////////////////////////////////////////////////////////////

   /**
    * @created    August 16, 2001
    */
   class TXState
   {
      byte          txState = TX_OPEN;
      LinkedList    sentMessages = new LinkedList();
      LinkedList    ackedMessages = new LinkedList();

      public String toString()
      {
         return super.toString() +
            "{ txState=" + txState +
            ", sendMessages=" + sentMessages +
            ", ackedMessages=" + ackedMessages +
            " }";
      }
   }
}
