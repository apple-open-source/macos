/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tm.usertx.interfaces;

import java.rmi.Remote;
import java.rmi.RemoteException;


/**
 *  The RMI remote UserTransaction session factory interface.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public interface UserTransactionSessionFactory
   extends Remote
{

   /**
    *  Create and return a new session.
    *
    *  @return A user transaction session.
    */
   public UserTransactionSession newInstance()
      throws RemoteException;
}
