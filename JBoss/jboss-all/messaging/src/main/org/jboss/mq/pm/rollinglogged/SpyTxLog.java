/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.rollinglogged;
import java.io.IOException;
import java.io.File;
import java.io.Serializable;

import javax.jms.JMSException;

import org.jboss.mq.SpyJMSException;

/**
 *  This is used to keep a log of commited transactions.
 *
 * @created    August 16, 2001
 * @author:    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version    $Revision: 1.6.4.1 $
 */
public class SpyTxLog {

   /////////////////////////////////////////////////////////////////////
   // Attributes
   /////////////////////////////////////////////////////////////////////
   private IntegrityLog transactionLog;
   private int      liveTransactionCount = 0;
   private Object   counterLock = new Object();

   /////////////////////////////////////////////////////////////////////
   // Constructors
   /////////////////////////////////////////////////////////////////////
   SpyTxLog( File file )
      throws JMSException {
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


   public void createTx()
      throws JMSException {
      synchronized ( counterLock ) {
         ++liveTransactionCount;
      }
   }

   public boolean completed()
      throws JMSException {
      synchronized ( counterLock ) {
         return ( liveTransactionCount == 0 );
      }
   }


   public synchronized void restore( java.util.TreeSet result )
      throws JMSException {
      try {
         result.addAll( transactionLog.toTreeSet() );
      } catch ( Exception e ) {
         throwJMSException( "Could not restore the transaction log.", e );
      }
   }

   public synchronized void commitTx( org.jboss.mq.pm.Tx id )
      throws JMSException {

      try {
         transactionLog.addTx( id );
         transactionLog.commit();
         synchronized ( counterLock ) {
            --liveTransactionCount;
         }
      } catch ( IOException e ) {
         throwJMSException( "Could not create a new transaction.", e );
      }

   }

   public void rollbackTx( org.jboss.mq.pm.Tx txId )
      throws JMSException {
      synchronized ( counterLock ) {
         --liveTransactionCount;
      }
   }


   /////////////////////////////////////////////////////////////////////
   // Private Methods
   /////////////////////////////////////////////////////////////////////
   private void throwJMSException( String message, Exception e )
      throws JMSException {
      JMSException newE = new SpyJMSException( message );
      newE.setLinkedException( e );
      throw newE;
   }
}
