// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: SerializableUserTransaction.java,v 1.1.4.1 2002/08/24 18:53:36 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

import java.rmi.RemoteException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.transaction.UserTransaction;

// utility for unambiguously shipping UserTransactions from node to node..

// NYI

// it looks like this will need proprietary info - the J2EE API does
// not give us enough... - I'm talking to Ole.

public class
  SerializableUserTransaction
  implements java.io.Serializable
{
  protected void
    log_warn(String message)
    {
      System.err.println("WARNING: "+message);
    }

  protected void
    log_error(String message, Exception e)
    {
      System.err.println("ERROR: "+message);
      e.printStackTrace(System.err);
    }

  protected Context _ctx=null;

  protected
    SerializableUserTransaction()
    throws RemoteException
    {
    }

  SerializableUserTransaction(UserTransaction userTransaction)
    throws RemoteException
    {
      log_warn("distribution of UserTransaction is NYI/Forbidden");
    }

  UserTransaction
    toUserTransaction()
    throws RemoteException
    {
      try
      {
	// optimise - TODO
	return (UserTransaction)new InitialContext().lookup("java:comp/UserTransaction");
      }
      catch (Exception e)
      {
	log_error("could not lookup UserTransaction", e);
	return null;
      }
    }
}
