/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.tm.resource;

import java.util.HashMap;

import javax.transaction.Transaction;
import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;

/**
 * A resource
 * @author Adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class Resource
   implements XAResource
{
   public static final int ERROR = -1;
   public static final int INACTIVE = 0;
   public static final int ACTIVE = 1;
   public static final int SUSPENDED = 2;
   public static final int PREPARED = 3;
   public static final int COMMITTED = 4;
   public static final int ROLLEDBACK = 5;
   public static final int FORGOT = 6;
   public static final int ENDED = 7;

   Integer id;

   boolean sameRM = false;

   HashMap xids = new HashMap();
   Xid last = null;

   public Resource(Integer id)
   {
      this.id = id;
   }

   public int getStatus()
      throws XAException
   {
      return getState(last, true, false).resState;
   }

   public void setStatus(int status)
      throws XAException
   {
      getState(last).status = status;
   }

   public void newResourceManager()
   {
      sameRM = false;
   }

   public int prepare(Xid xid)
      throws XAException
   {
      State state = getState(xid);
      if (state.resState != ACTIVE && state.resState != SUSPENDED && state.resState != ENDED)
      {
         state.resState = ERROR;
         throw new XAException(XAException.XAER_PROTO);
      }
      state.resState = PREPARED;
      return state.status;
   }

   public void commit(Xid xid, boolean onePhase)
      throws XAException
   {
      State state = getState(xid);
      if (onePhase == false && state.resState != PREPARED)
      {
         state.resState = ERROR;
         throw new XAException(XAException.XAER_PROTO);
      }
      if (onePhase && state.resState != ACTIVE && state.resState != SUSPENDED && state.resState != ENDED)
      {
         state.resState = ERROR;
         throw new XAException(XAException.XAER_PROTO);
      }
      state.resState = COMMITTED;
      state.removed = true;
   }

   public void rollback(Xid xid)
      throws XAException
   {
      State state = getState(xid);
      if (state.resState == INACTIVE)
      {
         state.resState = ERROR;
         throw new XAException(XAException.XAER_PROTO);
      }
      state.resState = ROLLEDBACK;
      state.removed = true;
   }

   public void forget(Xid xid)
      throws XAException
   {
      State state = getState(xid, false, false);
      state.resState = FORGOT;
      state.removed = true;
   }

   public void start(Xid xid, int flags)
      throws XAException
   {
      if (xid == null)
         throw new XAException(XAException.XAER_NOTA);
      if (flags == TMJOIN)
      {
         State state = getState(xid);
         return;
      }
      if (flags == TMRESUME)
      {
         State state = getState(xid);
         if (state.resState != SUSPENDED)
         {
            state.resState = ERROR;
            throw new XAException("Xid not suspended " + xid);
         }
         return;
      }
      if (xids.containsKey(xid))
      {
         throw new XAException(XAException.XAER_DUPID);
      }
      xids.put(xid, new State());
      last = xid;
      
   }

   public void end(Xid xid, int flags)
      throws XAException
   {
      State state = getState(xid);
      if (flags == TMSUSPEND)
      {
         state.resState = SUSPENDED;
         return;
      }
      state.resState = ENDED;
   }

   public Xid[] recover(int flag)
   {
      return new Xid[0];
   }

   public boolean isSameRM(XAResource res)
   {
      return sameRM;
   }

   public int getTransactionTimeout()
   {
      return 0;
   }

   public boolean setTransactionTimeout(int timeout)
   {
      return false;
   }

   public State getState(Xid xid)
      throws XAException
   {
      return getState(xid, false);
   }

   public State getState(Xid xid, boolean includeRemoved)
      throws XAException
   {
      return getState(xid, includeRemoved, true);
   }

   public State getState(Xid xid, boolean includeRemoved, boolean checkStatus)
      throws XAException
   {
      if (xid == null)
         throw new XAException(XAException.XAER_NOTA);
      State state = (State) xids.get(xid);
      if (state.resState == ERROR)
         throw new XAException(XAException.XAER_PROTO);
      if (state == null || (state.removed == true && includeRemoved == false))
         throw new XAException(XAException.XAER_PROTO);
      if (checkStatus && (state.status >= 5 || state.status < 0))
         throw new XAException(state.status);
      return state;
   }

   public class State
   {
      int status = XAResource.XA_OK;
      int resState = ACTIVE;
      boolean removed = false;
   }
}
