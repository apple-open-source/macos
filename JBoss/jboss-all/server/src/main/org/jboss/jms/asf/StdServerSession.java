/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.jms.asf;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.ServerSession;
import javax.jms.Session;
import javax.jms.XASession;
import javax.naming.InitialContext;
import javax.transaction.Status;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;

import org.jboss.logging.Logger;
import org.jboss.tm.TransactionManagerService;
import org.jboss.tm.XidFactoryMBean;

/**
 * An implementation of ServerSession. <p>
 *
 * Created: Thu Dec 7 18:25:40 2000
 *
 * @author    <a href="mailto:peter.antman@tim.se">Peter Antman</a> .
 * @author    <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a> .
 * @version   $Revision: 1.14.2.2 $
 */
public class StdServerSession
   implements Runnable, ServerSession, MessageListener
{
   /**
    * Instance logger.
    */
   static Logger log = Logger.getLogger(StdServerSession.class);
   
   /**
    * The server session pool which we belong to.
    */
   private StdServerSessionPool serverSessionPool;
   
   /**
    * Our session resource.
    */
   private Session session;
   
   /**
    * Our XA session resource.
    */
   private XASession xaSession;
   
   /**
    * The transaction manager that we will use for transactions.
    */
   private TransactionManager tm;
   
   /**
    * Use the session's XAResource directly if we have an JBossMQ XASession.
    * this allows us to get around the TX timeout problem when you have
    * extensive message processing.
    */
   private boolean useLocalTX;
   
   /**
    * The listener to delegate calls, to. In our case the container invoker.
    */
   private MessageListener delegateListener;

   private XidFactoryMBean xidFactory;
   
   /**
    * Create a <tt>StdServerSession</tt> .
    *
    * @param pool              The server session pool which we belong to.
    * @param session           Our session resource.
    * @param xaSession         Our XA session resource.
    * @param delegateListener  Listener to call when messages arrives.
    * @param useLocalTX       Will this session be used in a global TX (we can optimize with 1 phase commit)
    * @throws JMSException     Transation manager was not found.
    * @exception JMSException  Description of Exception
    */
   StdServerSession(final StdServerSessionPool pool,
                    final Session session,
                    final XASession xaSession,
                    final MessageListener delegateListener,
                    boolean useLocalTX,
                    final XidFactoryMBean xidFactory)
      throws JMSException
   {
      // assert pool != null
      // assert session != null
      
      this.serverSessionPool = pool;
      this.session = session;
      this.xaSession = xaSession;
      this.delegateListener = delegateListener;
      if( xaSession == null )
         useLocalTX = false;
      this.useLocalTX = useLocalTX;
      this.xidFactory = xidFactory;

      if (log.isDebugEnabled())
         log.debug("initializing (pool, session, xaSession, useLocalTX): " +
            pool + ", " + session + ", " + xaSession + ", " + useLocalTX);
      
      // Set out self as message listener
      if (xaSession != null)
         xaSession.setMessageListener(this);
      else
         session.setMessageListener(this);
      
      InitialContext ctx = null;
      try
      {
         ctx = new InitialContext();
         tm = (TransactionManager) ctx.lookup(TransactionManagerService.JNDI_NAME);
      }
      catch (Exception e)
      {
         throw new JMSException("Transation manager was not found");
      }
      finally
      {
         if (ctx != null)
         {
            try
            {
               ctx.close();
            }
            catch (Exception ignore)
            {
            }
         }
      }
   }
   
   // --- Impl of JMS standard API
   
   /**
    * Returns the session. <p>
    *
    * This simply returns what it has fetched from the connection. It is up to
    * the jms provider to typecast it and have a private API to stuff messages
    * into it.
    *
    * @return                  The session.
    * @exception JMSException  Description of Exception
    */
   public Session getSession() throws JMSException
   {
      if (xaSession != null)
         return xaSession;
      else
         return session;
   }
   
   //--- Protected parts, used by other in the package
   
   /**
    * Runs in an own thread, basically calls the session.run(), it is up to the
    * session to have been filled with messages and it will run against the
    * listener set in StdServerSessionPool. When it has send all its messages it
    * returns.
    */
   public void run()
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("running...");
      try
      {
         if (xaSession != null)
            xaSession.run();
         else
            session.run();
      }
      finally
      {
         if (trace)
            log.trace("recycling...");
        
         recycle();

         if (trace)
            log.trace("finished run");
      }
   }

   /**
    * Will get called from session for each message stuffed into it.
    *
    * Starts a transaction with the TransactionManager
    * and enlists the XAResource of the JMS XASession if a XASession was
    * available. A good JMS implementation should provide the XASession for use
    * in the ASF. So we optimize for the case where we have an XASession. So,
    * for the case where we do not have an XASession and the bean is not
    * transacted, we have the unneeded overhead of creating a Transaction. I'm
    * leaving it this way since it keeps the code simpler and that case should
    * not be too common (JBossMQ provides XASessions).
    */
   public void onMessage(Message msg)
   {      
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("onMessage running (pool, session, xaSession, useLocalTX): " +
         ", " + session + ", " + xaSession + ", " + useLocalTX);

      // Used if run with useLocalTX if true
      Xid localXid = null;
      boolean localRollbackFlag=false;
      // Used if run with useLocalTX if false
      Transaction trans = null;
      try
      {
         
         if (useLocalTX)
         {
            // Use JBossMQ One Phase Commit to commit the TX
            localXid = xidFactory.newXid();//new XidImpl();
            XAResource res = xaSession.getXAResource();
            res.start(localXid, XAResource.TMNOFLAGS);
            
            if( trace )
               log.trace("Using optimized 1p commit to control TX.");
         }
         else
         {
            
            // Use the TM to control the TX
            tm.begin();
            trans = tm.getTransaction();
            
            if (xaSession != null)
            {
               XAResource res = xaSession.getXAResource();
               if (!trans.enlistResource(res))
	       {
		  throw new JMSException("could not enlist resource");
	       }
               if( trace )
                  log.trace("XAResource '" + res + "' enlisted.");
            }
         }
         //currentTransactionId = connection.spyXAResourceManager.startTx();
         
         // run the session
         //session.run();
         // Call delegate listener
         delegateListener.onMessage(msg);
      }
      catch (Exception e)
      {
         log.error("session failed to run; setting rollback only", e);
         
         if (useLocalTX)
         {
            // Use JBossMQ One Phase Commit to commit the TX
            localRollbackFlag = true;
         }
         else
         {
            // Mark for tollback TX via TM
            try
            {
               // The transaction will be rolledback in the finally
               if( trace )
                  log.trace("Using TM to mark TX for rollback.");
               trans.setRollbackOnly();
            }
            catch (Exception x)
            {
               log.error("failed to set rollback only", x);
            }
         }
         
      }
      finally
      {
         try
         {
            if (useLocalTX)
            {
               if( localRollbackFlag == true  )
               {
                  if( trace )
                     log.trace("Using optimized 1p commit to rollback TX.");
                  
                  XAResource res = xaSession.getXAResource();
                  res.end(localXid, XAResource.TMSUCCESS);
                  res.rollback(localXid);
                  
               }
               else
               {
                  if( trace )
                     log.trace("Using optimized 1p commit to commit TX.");
                  
                  XAResource res = xaSession.getXAResource();
                  res.end(localXid, XAResource.TMSUCCESS);
                  res.commit(localXid, true);
               }
            }
            else
            {
               // Use the TM to commit the Tx (assert the correct association) 
               Transaction currentTx = tm.getTransaction();
               if (trans.equals(currentTx) == false)
                  throw new IllegalStateException("Wrong tx association: expected " + trans + " was " + currentTx);

               // Marked rollback
               if (trans.getStatus() == Status.STATUS_MARKED_ROLLBACK)
               {
                  if( trace )
                     log.trace("Rolling back JMS transaction");
                  // actually roll it back
                  tm.rollback();
                  
                  // NO XASession? then manually rollback.
                  // This is not so good but
                  // it's the best we can do if we have no XASession.
                  if (xaSession == null && serverSessionPool.isTransacted())
                  {
                     session.rollback();
                  }
               }
               else if (trans.getStatus() == Status.STATUS_ACTIVE)
               {
                  // Commit tx
                  // This will happen if
                  // a) everything goes well
                  // b) app. exception was thrown
                  if( trace )
                     log.trace("Commiting the JMS transaction");
                  tm.commit();

                  // NO XASession? then manually commit.  This is not so good but
                  // it's the best we can do if we have no XASession.
                  if (xaSession == null && serverSessionPool.isTransacted())
                  {
                     session.commit();
                  }
               }
            }            
         }
         catch (Exception e)
         {
            log.error("failed to commit/rollback", e);
         }
      }
      if( trace )
         log.trace("onMessage done");
   }

   /**
    * Start the session and begin consuming messages.
    *
    * @throws JMSException  No listener has been specified.
    */
   public void start() throws JMSException
   {
      log.trace("starting invokes on server session");
      
      if (session != null)
      {
         try
         {
            serverSessionPool.getExecutor().execute(this);
         }
         catch (InterruptedException ignore)
         {
         }
      }
      else
      {
         throw new JMSException("No listener has been specified");
      }
   }

   /**
    * Called by the ServerSessionPool when the sessions should be closed.
    */
   void close()
   {
      if (session != null)
      {
         try
         {
            session.close();
         }
         catch (Exception ignore)
         {
         }

         session = null;
      }

      if (xaSession != null)
      {
         try
         {
            xaSession.close();
         }
         catch (Exception ignore)
         {
         }
         xaSession = null;
      }

      log.debug("closed");
   }
   
   /**
    * This method is called by the ServerSessionPool when it is ready to be
    * recycled intot the pool
    */
   void recycle()
   {
      serverSessionPool.recycle(this);
   }
   
}

