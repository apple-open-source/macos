/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tm.usertx.interfaces;

import java.rmi.Remote;
import java.rmi.RemoteException;

import javax.transaction.UserTransaction;
import javax.transaction.NotSupportedException;
import javax.transaction.SystemException;
import javax.transaction.RollbackException;
import javax.transaction.HeuristicMixedException;
import javax.transaction.HeuristicRollbackException;


/**
 *  The RMI remote UserTransaction session interface.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public interface UserTransactionSession
   extends Remote
{
   /**
    *  Destroy this session.
    */
   public void destroy()
      throws RemoteException;

   /**
    *  Start a new transaction, and return its TPC.
    *
    *  @param timeout The timeout value for the new transaction, in seconds.
    *
    *  @return The transaction propagation context for the new transaction.
    */
   public Object begin(int timeout)
      throws RemoteException,
             NotSupportedException,
             SystemException;

   /**
    *  Commit the transaction.
    *
    *  @param tpc The transaction propagation context for the transaction.
    */
   public void commit(Object tpc)
      throws RemoteException,
             RollbackException,
             HeuristicMixedException,
             HeuristicRollbackException,
             SecurityException,
             IllegalStateException,
             SystemException;

   /**
    *  Rollback the transaction.
    *
    *  @param tpc The transaction propagation context for the transaction.
    */
   public void rollback(Object tpc)
      throws RemoteException,
             SecurityException,
             IllegalStateException,
             SystemException;

   /**
    *  Mark the transaction for rollback only.
    *
    *  @param tpc The transaction propagation context for the transaction.
    */
   public void setRollbackOnly(Object tpc)
      throws RemoteException,
             IllegalStateException,
             SystemException;
   
   /**
    *  Return status of the transaction.
    *
    *  @param tpc The transaction propagation context for the transaction.
    */
   public int getStatus(Object tpc)
      throws RemoteException,
             SystemException;
}
