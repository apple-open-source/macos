// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: J2EEWebApplicationHandler.java,v 1.1.4.2 2003/07/26 11:49:40 jules_gosnell Exp $
// ========================================================================

// A Jetty HttpServer with the interface expected by JBoss'
// J2EEDeployer...

//------------------------------------------------------------------------------

package org.mortbay.j2ee;

//------------------------------------------------------------------------------

import java.io.IOException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.transaction.Status;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import org.jboss.logging.Logger;
import org.mortbay.http.HttpRequest;
import org.mortbay.http.HttpResponse;
import org.mortbay.jetty.servlet.WebApplicationHandler;

//------------------------------------------------------------------------------

public abstract class
  J2EEWebApplicationHandler
  extends WebApplicationHandler
{
  protected static final Logger _log=Logger.getLogger(J2EEWebApplicationHandler.class);

  protected Context            _ctx;
  protected TransactionManager _tm;

  public void
    start()
    {
      try
      {
	super.start();

	_ctx=new InitialContext();
	_tm=(TransactionManager)_ctx.lookup("java:/TransactionManager");
      }
      catch (Exception e)
      {
	_log.error("could not find TransactionManager", e);
      }

    }

  public void handle(String pathInContext,
		     String pathParams,
		     HttpRequest httpRequest,
		     HttpResponse httpResponse)
    throws IOException
    {
      //      _log.info("HANDLE()...");
      try
      {
	super.handle(pathInContext, pathParams, httpRequest, httpResponse);
      }
      finally
      {
	disassociateTransaction();
	disassociateSecurity();
      }
      //      _log.info("...HANDLE()");
    }

  protected void
    disassociateTransaction()
    {
      int status=Status.STATUS_NO_TRANSACTION;
      try
      {
	status=_tm.getStatus();
      }
      catch (Exception e)
      {
	_log.error("could not get status of current Transaction", e);
      }

      // these are the possible statuses:
      // STATUS_ACTIVE
      // STATUS_COMMITTED
      // STATUS_COMMITTING
      // STATUS_MARKED_ROLLBACK
      // STATUS_NO_TRANSACTION
      // STATUS_PREPARED
      // STATUS_PREPARING
      // STATUS_ROLLEDBACK
      // STATUS_ROLLING_BACK
      // STATUS_UNKNOWN

      if (!(status==Status.STATUS_COMMITTED ||
	    status==Status.STATUS_NO_TRANSACTION ||
	    status==Status.STATUS_ROLLEDBACK))
      {
	// we should rollback this transactions and disassociate it
	// from the current thread...
	try
	{
	  _log.warn("UserTransactions MUST be completed by end of service() method");
	  _log.warn("Rolling back incomplete transaction on current thread");
	  _tm.rollback();
	}
	catch (Exception e)
	{
	  _log.error("could not rollback incomplete transaction", e);
	}
      }

      Transaction garbage=null;
      try
      {
	garbage=_tm.suspend();
      }
      catch (Exception e)
      {
	_log.error("could not disassociate transaction context from current thread", e);
      }

      garbage=null;
    }

  protected abstract void disassociateSecurity();
}
