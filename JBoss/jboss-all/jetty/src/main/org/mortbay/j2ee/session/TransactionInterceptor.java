// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: TransactionInterceptor.java,v 1.1.4.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.servlet.http.HttpSession;
import javax.transaction.InvalidTransactionException;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import org.jboss.logging.Logger;

//----------------------------------------

// We need to ensure that calls to the HttpSession implementation are
// made in Jetty's and not the User's Transaction Context. Otherwise
// if their transaction is rolledback, our state is lost and
// vice-versa...

public class TransactionInterceptor
  extends AroundInterceptor
{
  protected static final Logger _log=Logger.getLogger(TransactionInterceptor.class);
  protected final ThreadLocal _theirTransaction =new ThreadLocal();
  protected       Context     _ctx;

  public
    TransactionInterceptor()
  {
    super();

    try
    {
      _ctx=new InitialContext();
    }
    catch (Exception e)
    {
      _log.error("could not create InitialContext", e);
    }
  }

  protected TransactionManager
    getTransactionManager()
  {
    try
    {
      return (TransactionManager)_ctx.lookup("java:/TransactionManager");
    }
    catch (NamingException e)
    {
      _log.error("could not find TransactionManager", e);
    }

    return null;
  }

  // despite the names (push/pop) these are not expected to be reentrant....
  protected void
    before()
  {
    try
    {
      Transaction tx=getTransactionManager().suspend();
      _theirTransaction.set(tx);
    }
    catch (SystemException e)
    {
      _log.error("could not disassociate UserTransaction from current thread", e);
    }
  }

  protected void
    after()
  {
    try
    {
      Transaction tx=(Transaction)_theirTransaction.get();
      getTransactionManager().resume(tx);
    }
    catch (Exception e)
    {
      _log.error("could not re-associate UserTransaction with current thread", e);
    }
    finally
    {
      _theirTransaction.set(null);
    }
  }

  //  public Object clone() { return this; } // Stateless - Context should be valid for whole webapp
}
