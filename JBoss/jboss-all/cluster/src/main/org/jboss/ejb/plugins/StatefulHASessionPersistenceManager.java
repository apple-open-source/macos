/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import java.rmi.RemoteException;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.IOException;

import javax.ejb.SessionBean;
import javax.ejb.RemoveException;
import javax.ejb.EJBException;

import javax.naming.Context;
import javax.naming.InitialContext;

import org.jboss.ejb.Container;
import org.jboss.ejb.StatefulSessionContainer;
import org.jboss.ejb.StatefulSessionPersistenceManager;
import org.jboss.ejb.StatefulSessionEnterpriseContext;
import org.jboss.metadata.ClusterConfigMetaData;

import org.jboss.ha.hasessionstate.interfaces.HASessionState;
import org.jboss.ha.hasessionstate.interfaces.PackagedSession;
import org.jboss.ha.framework.interfaces.HAPartition;

import org.jboss.system.ServiceMBeanSupport;

import org.jboss.util.id.UID;
// import org.jboss.util.collection.CompoundKey;

/**
 *  This persistence manager work with an underlying HASessionState to get
 *  clustered state.
 *
 *  @see HASessionState
 *  @see org.jboss.ha.hasessionstate.server.HASessionStateImpl
 *  
 *  @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 *  @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 *  @version $Revision: 1.7.2.4 $
 *
 *   <p><b>Revisions:</b>
 */

public class StatefulHASessionPersistenceManager
    extends ServiceMBeanSupport
    implements StatefulSessionPersistenceManager,
	       HASessionState.HASessionStateListener,
	       HAPersistentManager
{
   
   /** Creates new StatefulHASessionPersistenceManager */
   public StatefulHASessionPersistenceManager ()
   {
   }
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------

   private StatefulSessionContainer con;
   
   private HASessionState sessionState = null;
   
   private String localNodeName = null;
   private String appName = null;
   
   // Static --------------------------------------------------------

   private static String DEFAULT_HASESSIONSTATE_JNDI_NAME = "/HASessionState/Default";
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------

   public void setContainer (Container c)
   {
      con = (StatefulSessionContainer)c;
   }

   protected void createService() throws Exception
   {
      // Initialize the dataStore

      // Find HASessionState that we will use
      //
      String sessionStateName = org.jboss.metadata.ClusterConfigMetaData.DEFAULT_SESSION_STATE_NAME;
      ClusterConfigMetaData config = con.getBeanMetaData ().getClusterConfigMetaData ();
      if (config != null)
         sessionStateName = config.getHaSessionStateName ();

      Context ctx = new InitialContext ();
      try {
         this.sessionState = (HASessionState)ctx.lookup (sessionStateName);
      }
      finally {
         ctx.close();
      }
      
      this.localNodeName = this.sessionState.getNodeName ();
      this.appName = this.con.getBeanMetaData ().getJndiName ();
      
      this.sessionState.subscribe (this.appName, this);
   }
   
   protected void stopService ()
   {
      this.sessionState.unsubscribe (this.appName, this);
   }

   public Object createId(StatefulSessionEnterpriseContext ctx)
      throws Exception
   {
      //
      // jason: could probably use org.jboss.util.collection.CompoundKey here
      //        for better uniqueness based on hashCode and such...
      //
      // return new CompoundKey(this.localNodeName, new UID());
      //
      return this.localNodeName + ":" + new UID();
   }

   public void createdSession(StatefulSessionEnterpriseContext ctx)
      throws Exception
   {
      this.sessionState.createSession(this.appName, ctx.getId());
   }
   
   public void activateSession (StatefulSessionEnterpriseContext ctx) throws RemoteException
   {
      try
      {
         ObjectInputStream in;
         
         // Load state
         PackagedSession state = this.sessionState.getStateWithOwnership (this.appName, ctx.getId ());
         
         if (state == null)
            throw new EJBException ("Could not activate; failed to recover session (session has been probably removed by session-timeout)");
         
         in = new SessionObjectInputStream (ctx, new ByteArrayInputStream (state.getState ()));;
         
         ctx.setInstance ( in.readObject () );
         
         in.close ();
         
         //removePassivated (ctx.getId ());

         
         // Instruct the bean to perform activation logic
         ((SessionBean)ctx.getInstance()).ejbActivate();
      }
      catch (ClassNotFoundException e)
      {
         throw new EJBException ("Could not activate", e);
      }
      catch (IOException e)
      {
         throw new EJBException ("Could not activate", e);
      }
   }
   
   public void passivateSession (StatefulSessionEnterpriseContext ctx)
      throws RemoteException
   {
      // do nothing
   }
   
   public void synchroSession (StatefulSessionEnterpriseContext ctx) throws RemoteException
   {
      try
      {
         // Call bean
         ((SessionBean)ctx.getInstance()).ejbPassivate();
         
         // Store state
         ByteArrayOutputStream baos = new ByteArrayOutputStream ();
         ObjectOutputStream out = new SessionObjectOutputStream (baos);
         
         out.writeObject (ctx.getInstance ());
         
         out.close ();
         
         this.sessionState.setState (this.appName, ctx.getId (), baos.toByteArray ());

         ((SessionBean)ctx.getInstance()).ejbActivate(); //needed?         
      }
      catch (IOException e)
      {
         throw new EJBException ("Could not passivate", e);
      }
   }
   
   public void removeSession (StatefulSessionEnterpriseContext ctx)
      throws RemoteException, RemoveException
   {
      try
      {
         // Call bean
         ((SessionBean)ctx.getInstance()).ejbRemove();
      }
      finally
      {
         this.sessionState.removeSession (this.appName, ctx.getId ());         
      }
   }
   
   public void removePassivated (Object key)
   {
      this.sessionState.removeSession (this.appName, key);
   }
   
   // HASessionState.HASessionStateListener methods -----------------
   //
   public void sessionExternallyModified (PackagedSession session)
   {
      // this callback warns us that a session (i.e. a bean) has been externally modified
      // this means that we need to tell to our InstanceCache that this bean is no more under its control
      // and that it should not keep a cached version of it => CACHE MISS
      //
      log.trace ("Invalidating cache for session: " + session.getKey());
      this.con.getInstanceCache ().remove (session.getKey ());
   }
   
   public void newSessionStateTopology (HAPartition haSubPartition)
   {
   }
   
}
