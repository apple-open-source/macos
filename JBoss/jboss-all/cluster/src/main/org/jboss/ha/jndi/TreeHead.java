/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.jndi;


import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.logging.Logger;
import org.jnp.interfaces.Naming;
import org.jnp.interfaces.NamingContext;

import javax.naming.CommunicationException;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NameNotFoundException;
import javax.naming.NamingException;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collection;

/**
 *   This class extends the JNP JNDI implementation.
 *   binds and unbinds will be distributed to all members of the cluster
 *   that are running HAJNDI.
 *   lookups will look for Names in HAJNDI then delegate to the local InitialContext
 *   This class is fully serializable for GET_STATE
 *
 *   @author <a href="mailto:sacha.labourey@jboss.org">Sacha Labourey</a>
 *   @version $Revision: 1.1.2.1 $
 */
public class TreeHead extends org.jnp.server.NamingServer
   implements Serializable, org.jnp.interfaces.Naming
{
   // Attributes --------------------------------------------------------
   private static Logger log = Logger.getLogger(TreeHead.class);

   private transient HAPartition partition;
   private transient HAJNDI father;

   // Constructor --------------------------------------------------------

   public TreeHead ()
      throws NamingException
   {
      super();
   }

   // Public --------------------------------------------------------

   public void init() throws Exception
   {
      log.debug("registerRPCHandler");
      partition.registerRPCHandler("HAJNDI", this);
   }

   public void stop() throws Exception
   {
      log.debug("unregisterRPCHandler");
      partition.unregisterRPCHandler("HAJNDI", this);
   }

   public void setPartition (HAPartition partition)
   {
      this.partition = partition;
   }

   public void setHARMIHead (HAJNDI father)
   {
      this.father = father;
   }

   // Naming implementation -----------------------------------------


   public synchronized void _bind(Name name, Object obj, String className)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("_bind, name="+name);
      super.bind(name, obj, className);
   }
   public synchronized void bind(Name name, Object obj, String className)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("bind, name="+name);
      super.bind(name, obj, className);
      // if we get here, this means we can do it on every node.
      Object[] args = new Object[3];
      args[0] = name;
      args[1] = obj;
      args[2] = className;
      try
      {
         partition.callMethodOnCluster("HAJNDI", "_bind", args, true);
      }
      catch (Exception e)
      {
         CommunicationException ce = new CommunicationException("Failed to _bind on cluster");
         ce.setRootCause(e);
         throw ce;
      }
   }

   public synchronized void _rebind(Name name, Object obj, String className)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("_rebind, name="+name);
      super.rebind(name, obj, className);
   }
   public synchronized void rebind(Name name, Object obj, String className)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("rebind, name="+name);
      super.rebind(name, obj, className);

      // if we get here, this means we can do it on every node.
      Object[] args = new Object[3];
      args[0] = name;
      args[1] = obj;
      args[2] = className;
      try
      {
         partition.callMethodOnCluster("HAJNDI", "_rebind", args, true);
      }
      catch (Exception e)
      {
         CommunicationException ce = new CommunicationException("Failed to _rebind on cluster");
         ce.setRootCause(e);
         throw ce;
      }
   }

   public synchronized void _unbind(Name name)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.debug("_unbind, name="+name);
      super.unbind(name);
   }
   public synchronized void unbind(Name name)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("unbind, name="+name);
      super.unbind(name);

      // if we get here, this means we can do it on every node.
      Object[] args = new Object[1];
      args[0] = name;
      try
      {
         partition.callMethodOnCluster("HAJNDI", "_unbind", args, true);
      }
      catch (Exception e)
      {
         CommunicationException ce = new CommunicationException("Failed to _unbind on cluster");
         ce.setRootCause(e);
         throw ce;
      }
   }

   public Object lookup(Name name)
      throws NamingException
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("lookup, name="+name);
      Object result = null;
      try
      {
         result = super.lookup(name);
      }
      catch (NameNotFoundException ex)
      {
         try
         {
            // not found in global jndi, look in local.
            result = lookupLocally(name);
         }
         catch (NameNotFoundException nnfe)
         {
            // if we get here, this means we can do it on every node.
            Object[] args = new Object[1];
            args[0] = name;
            ArrayList rsp = null;
            Exception cause = null;
            try
            {
               if( trace )
                  log.trace("calling lookupLocally("+name+") on cluster");
               rsp = partition.callMethodOnCluster("HAJNDI", "lookupLocally", args, true);
            }
            catch (Exception ignored)
            {
               if( trace )
                  log.trace("Clusterd lookupLocally("+name+") failed", ignored);
               cause = ignored;
            }

            if (rsp == null || rsp.size() == 0)
            {
               NameNotFoundException nnfe2 = new NameNotFoundException(name.toString());
               nnfe2.setRootCause(cause);
               throw nnfe2;
            }

            for (int i = 0; i < rsp.size(); i++)
            {
               result = rsp.get(i);
               if( result != null )
                  log.trace("_lookupLocally returned: " + result.getClass().getName());
               if (!(result instanceof Exception))
                  return result;
            }
            throw nnfe;
         }
      }
      return result;
   }

   public Object _lookupLocally(Name name)
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("_lookupLocally, name="+name);
      try
      {
         return lookupLocally(name);
      }
      catch (Exception e)
      {
         if( trace )
            log.trace("_lookupLocally failed", e);
         return e;
      }
   }

   public Object lookupLocally(Name name) throws NamingException
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("lookupLocally, name="+name);

      // TODO: This is a really big hack here
      // We cannot do InitialContext().lookup(name) because
      // we get ClassNotFound errors and ClassLinkage errors.
      // So, what we do is cheat and get the static localServer variable
      try
      {
         if (NamingContext.localServer != null)
         {
            return NamingContext.localServer.lookup(name);
         }
         else
         {
            InitialContext ctx = new InitialContext();
            return ctx.lookup(name);
         }
      }
      catch (NamingException e)
      {
         if( trace )
            log.trace("lookupLocally failed, name=" + name, e);
         throw e;
      }
      catch (java.rmi.RemoteException e)
      {
         NamingException ne = new NamingException("unknown remote exception");
         ne.setRootCause(e);
         if( trace )
            log.trace("lookupLocally failed, name=" + name, e);
         throw ne;
      }
      catch (RuntimeException e)
      {
         if( trace )
            log.trace("lookupLocally failed, name=" + name, e);
         throw e;
      }
   }

   protected ArrayList enum2list (javax.naming.NamingEnumeration en)
   {
      ArrayList rtn = new ArrayList();
      try
      {
         while (en.hasMore())
         {
            rtn.add(en.next());
         }
         en.close();
      }
      catch (NamingException ignored) {}
      return rtn;
   }

   public Collection list(Name name)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("list, name="+name);
      Collection result = null;
      try
      {
         result = super.list(name);
      }
      catch (NameNotFoundException ex)
      {
         // not found in global jndi, look in local.
         result =  enum2list(new InitialContext().list(name));
      }
      return result;
   }

   public Collection listBindings(Name name)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("listBindings, name="+name);
      Collection result = null;
      try
      {
         result = super.listBindings(name);
      }
      catch (NameNotFoundException ex)
      {
         // not found in global jndi, look in local.
         result =  enum2list(new InitialContext().listBindings(name));
      }
      return result;
   }

   public synchronized javax.naming.Context _createSubcontext(Name name)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("_createSubcontext, name="+name);
      return super.createSubcontext(name);
   }
   public synchronized javax.naming.Context createSubcontext(Name name)
      throws NamingException
   {
      if( log.isTraceEnabled() )
         log.trace("createSubcontext, name="+name);
      javax.naming.Context result = super.createSubcontext(name);

      // if we get here, this means we can do it on every node.
      Object[] args = new Object[1];
      args[0] = name;
      try
      {
         partition.callMethodOnCluster("HAJNDI", "_createSubcontext", args, true);
      }
      catch (Exception e)
      {
         CommunicationException ce = new CommunicationException("Failed to _createSubcontext on cluster");
         ce.setRootCause(e);
         throw ce;
      }

      return result;
   }

   public Naming getRoot ()
   {
      return father.getHAStub();
   }

}
