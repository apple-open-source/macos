
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.adapter; 

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.ConnectionEventListener;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.LocalTransaction;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionMetaData;
import javax.security.auth.Subject;
import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;
import org.jboss.logging.Logger;

// Generated package name


/**
 * TestManagedConnection.java
 *
 *
 * Created: Mon Dec 31 17:07:16 2001
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestManagedConnection  implements ManagedConnection, XAResource 
{
   private final int id;

   private Logger log = Logger.getLogger(getClass());
   private List handles = new LinkedList();
   private List listeners = new LinkedList();
   private Subject subject;
   private TestConnectionRequestInfo cri;

   private Xid xid;

   private boolean destroyed = false;

   private boolean failInPrepare = false;
   private boolean failInCommit = false;
   private int xaCode;

   public TestManagedConnection (final Subject subject, final TestConnectionRequestInfo cri, final int id)
   {
      this.subject = subject;
      this.cri = cri;
      this.id = id;
   }

   void setFailInPrepare(final boolean fail, final int xaCode)
   {
      this.failInPrepare = fail;
      this.xaCode = xaCode;
   }

   void setFailInCommit(final boolean fail, final int xaCode)
   {
      this.failInCommit = fail;
      this.xaCode = xaCode;
   }

   // implementation of javax.resource.spi.ManagedConnection interface

   /**
    *
    * @exception javax.resource.ResourceException <description>
    */
   public void destroy() throws ResourceException
   {
      log.info("Destroying connection");
      cleanup();
      destroyed = true;
   }

   /**
    *
    * @exception javax.resource.ResourceException <description>
    */
   public void cleanup() throws ResourceException
   {
      for (Iterator i = handles.iterator(); i.hasNext(); )
      {
         TestConnection c = (TestConnection)i.next();
         c.setMc(null);
         i.remove();
      } // end of for ()
      
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Object getConnection(Subject param1, ConnectionRequestInfo param2) throws ResourceException
   {
      if (param2 != null && ((TestConnectionRequestInfo) param2).failure.equals("getConnectionResource"))
         throw new ResourceException("");
      if (param2 != null && ((TestConnectionRequestInfo) param2).failure.equals("getConnectionRuntime"))
         throw new RuntimeException("");
      TestConnection c =  new TestConnection(this);
      handles.add(c);
      return c;
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.resource.ResourceException <description>
    */
   public void associateConnection(Object p) throws ResourceException
   {
      if (p instanceof TestConnection) 
      {
         ((TestConnection)p).setMc(this);
         handles.add(p);
      } // end of if ()
      else
      {
         throw new ResourceException("wrong kind of Connection");         
      } // end of else
      
   }

   /**
    *
    * @param param1 <description>
    */
   public void addConnectionEventListener(ConnectionEventListener cel)
   {
      log.info("adding 1 cel");
      listeners.add(cel);
   }

   /**
    *
    * @param param1 <description>
    */
   public void removeConnectionEventListener(ConnectionEventListener cel)
   {
      log.info("removing 1 cel");
      listeners.remove(cel);
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public XAResource getXAResource() throws ResourceException
   {
     return this;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public LocalTransaction getLocalTransaction() throws ResourceException
   {
     // TODO: implement this javax.resource.spi.ManagedConnection method
     return null;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public ManagedConnectionMetaData getMetaData() throws ResourceException
   {
     // TODO: implement this javax.resource.spi.ManagedConnection method
     return null;
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.resource.ResourceException <description>
    */
   public void setLogWriter(PrintWriter param1) throws ResourceException
   {
     // TODO: implement this javax.resource.spi.ManagedConnection method
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public PrintWriter getLogWriter() throws ResourceException
   {
     // TODO: implement this javax.resource.spi.ManagedConnection method
     return null;
   }

   // implementation of javax.transaction.xa.XAResource interface

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void start(Xid xid, int flags) throws XAException
   {
      log.info("start with xid " + xid + ", flags " + flags);
      this.xid = xid;
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void end(final Xid xid, final int flags) throws XAException
   {
      log.info("end with xid " + xid + ", flags " + flags);
      if (!xid.equals(this.xid)) 
      {
         log.info("wrong xid ended: have " + this.xid + ", got: " + xid);
      } // end of if ()
      this.xid = null;
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void commit(Xid xid, boolean onePhase) throws XAException
   {
      log.info("commit with xid " + xid);
      if (failInCommit) 
      {
         throw new XAException(xaCode);
      } // end of if ()
      // do nothing
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void rollback(Xid xid) throws XAException
   {
      log.info("rollback with xid " + xid);
      // do nothing
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public int prepare(Xid xid) throws XAException
   {
      log.info("prepare with xid " + xid);
      if (failInPrepare) 
      {
         throw new XAException(xaCode);
      } // end of if ()
      
      // do nothing
      return 0;
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void forget(Xid xid) throws XAException
   {
      log.info("forget with xid " + xid);
      // nothing doing
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public Xid[] recover(int param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return null;
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public boolean isSameRM(XAResource xar) throws XAException
   {
      return this == xar;
   }

   /**
    *
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public int getTransactionTimeout() throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return 0;
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public boolean setTransactionTimeout(int param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return false;
   }

   boolean isInTx()
   {
      log.info("isInTx, xid: " + xid);
      return xid != null;
   }

   void connectionClosed(TestConnection handle)
   {
      if (destroyed)
         return;

      ConnectionEvent ce = new ConnectionEvent(this ,ConnectionEvent.CONNECTION_CLOSED);
      ce.setConnectionHandle(handle);
      Collection copy = new ArrayList(listeners);
      for (Iterator i = copy.iterator(); i.hasNext(); )
      {
         log.info("notifying 1 cel connectionClosed");
         ConnectionEventListener cel = (ConnectionEventListener)i.next();
         try
         {
            cel.connectionClosed(ce);
         }
         catch (Throwable ignored)
         {
            ignored.printStackTrace();
         }
      } // end of for ()
      handles.remove(handle);
   }

   void connectionError(TestConnection handle, Exception e)
   {
      ConnectionEvent ce = new ConnectionEvent(this, ConnectionEvent.CONNECTION_ERROR_OCCURRED, e);
      ce.setConnectionHandle(handle);
      Collection copy = new ArrayList(listeners);
      for (Iterator i = copy.iterator(); i.hasNext(); )
      {
         ConnectionEventListener cel = (ConnectionEventListener)i.next();
         try
         {
            cel.connectionErrorOccurred(ce);
         }
         catch (Throwable ignored)
         {
         }
      } // end of for ()
   }

   public String toString()
   {
      return "tmc: " + id;
   }

}
