/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tm.usertx.server;

import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;

import org.jboss.tm.usertx.interfaces.UserTransactionSession;
import org.jboss.tm.usertx.interfaces.UserTransactionSessionFactory;


/**
 *  The RMI remote UserTransaction session factory implementation.
 */
public class UserTransactionSessionFactoryImpl
   extends UnicastRemoteObject
   implements UserTransactionSessionFactory
{
   /**
    *  A no-args constructor that throws <code>RemoteException</code>.
    */
   public UserTransactionSessionFactoryImpl()
      throws RemoteException
   {
      super();
   }

   //
   // implements interface UserTransactionSessionFactory
   //

   /**
    *  Create and return a new session.
    *
    *  @return A new user transaction session.
    */
   public UserTransactionSession newInstance()
      throws RemoteException
   {
      return new UserTransactionSessionImpl();
   }
}
