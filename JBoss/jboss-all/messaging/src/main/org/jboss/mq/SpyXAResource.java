/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq;

import javax.jms.JMSException;

import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;

import org.jboss.logging.Logger;

/**
 *  This class implements the XAResouece interface for used with an XASession.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.5.2.3 $
 */
public class SpyXAResource
   implements XAResource
{
   private static final Logger log = Logger.getLogger(SpyXAResource.class);
   
   //////////////////////////////////////////////////////////////////
   // Attributes
   //////////////////////////////////////////////////////////////////

   SpySession       session;

   //////////////////////////////////////////////////////////////////
   // Constructors
   //////////////////////////////////////////////////////////////////

   SpyXAResource( SpySession session ) {
      this.session = session;
   }

   /**
    *  setTransactionTimeout method comment.
    *
    * @param  arg1                                  The new TransactionTimeout
    *      value
    * @return                                       Description of the Returned
    *      Value
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public boolean setTransactionTimeout( int arg1 )
      throws javax.transaction.xa.XAException {
      return false;
   }

   /**
    *  getTransactionTimeout method comment.
    *
    * @return                                       The TransactionTimeout value
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public int getTransactionTimeout()
      throws javax.transaction.xa.XAException {
      return 0;
   }

   /**
    *  isSameRM method comment.
    *
    * @param  arg1                                  Description of Parameter
    * @return                                       The SameRM value
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public boolean isSameRM( javax.transaction.xa.XAResource arg1 )
      throws javax.transaction.xa.XAException {
      if ( !( arg1 instanceof SpyXAResource ) ) {
         return false;
      }
      return ( ( SpyXAResource )arg1 ).session.connection.spyXAResourceManager == session.connection.spyXAResourceManager;
   }

   //////////////////////////////////////////////////////////////////
   // Public Methods
   //////////////////////////////////////////////////////////////////

   /**
    *  commit method comment.
    *
    * @param  xid                                   Description of Parameter
    * @param  onePhase                              Description of Parameter
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public void commit( javax.transaction.xa.Xid xid, boolean onePhase )
      throws javax.transaction.xa.XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Commit xid=" + xid + ", onePhase=" + onePhase);
      }
      
      try {
         session.connection.spyXAResourceManager.commit( xid, onePhase );
      } catch ( JMSException e ) {
         throw new SpyXAException( XAException.XAER_RMERR, e );
      }
   }

   /**
    *  end method comment.
    *
    * @param  xid                                   Description of Parameter
    * @param  flags                                 Description of Parameter
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public void end( javax.transaction.xa.Xid xid, int flags )
      throws javax.transaction.xa.XAException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("End xid=" + xid + ", flags=" + flags);

      synchronized ( session.runLock ) {

         switch ( flags ) {
            case TMSUSPEND:
               session.unsetCurrentTransactionId(xid);
               session.connection.spyXAResourceManager.suspendTx( xid );
               break;
            case TMFAIL:
               session.unsetCurrentTransactionId(xid);
               session.connection.spyXAResourceManager.endTx( xid, false );
               break;
            case TMSUCCESS:
               session.unsetCurrentTransactionId(xid);
               session.connection.spyXAResourceManager.endTx( xid, true );
               break;
         }
      }
   }

   /**
    *  forget method comment.
    *
    * @param  arg1                                  Description of Parameter
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public void forget( javax.transaction.xa.Xid xid )
      throws javax.transaction.xa.XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Forget xid=" + xid);
      }
   }

   /**
    *  prepare method comment.
    *
    * @param  xid                                   Description of Parameter
    * @return                                       Description of the Returned
    *      Value
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public int prepare( javax.transaction.xa.Xid xid )
      throws javax.transaction.xa.XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Prepare xid=" + xid);
      }
      
      try {
         return session.connection.spyXAResourceManager.prepare( xid );
      } catch ( JMSException e ) {
         throw new SpyXAException( XAException.XAER_RMERR, e );
      }
   }

   /**
    *  recover method comment.
    *
    * @param  arg1                                  Description of Parameter
    * @return                                       Description of the Returned
    *      Value
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public Xid[] recover( int arg1 )
      throws javax.transaction.xa.XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Recover arg1=" + arg1);
      }
      
      return new Xid[0];
   }

   /**
    *  rollback method comment.
    *
    * @param  xid                                   Description of Parameter
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public void rollback( javax.transaction.xa.Xid xid )
      throws javax.transaction.xa.XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Rollback xid=" + xid);
      }
      
      try {
         session.connection.spyXAResourceManager.rollback( xid );
      } catch ( JMSException e ) {
         throw new SpyXAException( XAException.XAER_RMERR, e );
      }
   }

   /**
    *  start method comment.
    *
    * @param  xid                                   Description of Parameter
    * @param  flags                                 Description of Parameter
    * @exception  javax.transaction.xa.XAException  Description of Exception
    */
   public void start( javax.transaction.xa.Xid xid, int flags )
      throws javax.transaction.xa.XAException
   {
      if (log.isTraceEnabled()) {
         log.trace("Start xid=" + xid + ", flags=" + flags);
      }

      boolean convertTx=false;
      if ( session.getCurrentTransactionId() != null ) {
	 if( flags==TMNOFLAGS && session.getCurrentTransactionId() instanceof Long ) { 
            convertTx=true;
         } else {
            throw new XAException( XAException.XAER_OUTSIDE );
	 }
      }

      synchronized ( session.runLock ) {

         switch ( flags ) {
            case TMNOFLAGS:
	       if( convertTx ) {
                  // it was an anonymous TX, TM is now taking control over it.
	          // convert it over to a normal XID tansaction.
                  session.setCurrentTransactionId(session.connection.spyXAResourceManager.convertTx( (Long)session.getCurrentTransactionId(), xid ));
	       } else {
                  session.setCurrentTransactionId(session.connection.spyXAResourceManager.startTx( xid ));
               }
               break;
            case TMJOIN:
               session.setCurrentTransactionId(session.connection.spyXAResourceManager.joinTx( xid ));
               break;
            case TMRESUME:
               session.setCurrentTransactionId(session.connection.spyXAResourceManager.resumeTx( xid ));
               break;
         }
         session.runLock.notify();

      }

   }
}
