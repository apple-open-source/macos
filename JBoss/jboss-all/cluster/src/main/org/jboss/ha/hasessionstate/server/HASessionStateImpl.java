/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.hasessionstate.server;

import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.hasessionstate.interfaces.PackagedSession;

import org.jboss.logging.Logger;

import java.util.Iterator;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Enumeration;
import java.util.Vector;
import java.io.Serializable;
import java.io.IOException;
import java.util.zip.InflaterInputStream;
import java.util.zip.Deflater;
import java.util.zip.DeflaterOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectInputStream;

import javax.naming.InitialContext;
import javax.naming.NameNotFoundException;
import javax.naming.Reference;
import javax.naming.StringRefAddr;
import javax.naming.Name;
import javax.naming.Context;
import javax.naming.NameNotFoundException;

import org.jboss.naming.NonSerializableFactory;

import EDU.oswego.cs.dl.util.concurrent.Mutex;

/**
 *   Default implementation of HASessionState
 *
 *   @see org.jboss.ha.hasessionstate.interfaces.HASessionState
 *   @author sacha.labourey@cogito-info.ch
 *   @author <a href="bill@burkecentral.com">Bill Burke</a>
 *   @version $Revision: 1.8.2.5 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2002/01/09: billb</b>
 * <ol>
 *   <li>ripped out sub partitioning stuff.  It really belongs as a subclass of HAPartition
 * </ol>
 * 
 */

public class HASessionStateImpl
   implements org.jboss.ha.hasessionstate.interfaces.HASessionState,
	      HAPartition.HAPartitionStateTransfer
{
   
   protected String _sessionStateName;
   protected Logger log;
   protected HAPartition hapGeneral;
   protected String sessionStateIdentifier;
   protected String myNodeName;
   
   protected long beanCleaningDelay;
   protected String haPartitionName;
   protected String haPartitionJndiName;
   
   protected final String DEFAULT_PARTITION_JNDI_NAME = org.jboss.metadata.ClusterConfigMetaData.DEFAULT_PARTITION;
   protected final String JNDI_FOLDER_NAME_FOR_HASESSIONSTATE = org.jboss.metadata.ClusterConfigMetaData.JNDI_PREFIX_FOR_SESSION_STATE;
   protected final String JNDI_FOLDER_NAME_FOR_HAPARTITION = "/HAPartition/";
   protected final long MAX_DELAY_BEFORE_CLEANING_UNRECLAIMED_STATE = 30L * 60L * 1000L; // 30 minutes... should be set externally or use cache settings
   protected static final String HA_SESSION_STATE_STATE_TRANSFER = "HASessionStateTransfer";
   
   protected HashMap locks = new HashMap ();
      
   public HASessionStateImpl ()
   {}
   
   public HASessionStateImpl (String sessionStateName,
                              String mainHAPartitionName,
                              long beanCleaningDelay)
   {
      if (sessionStateName == null)
         this._sessionStateName = org.jboss.metadata.ClusterConfigMetaData.DEFAULT_SESSION_STATE_NAME;
      else
         this._sessionStateName = sessionStateName;
      
      this.sessionStateIdentifier = "SessionState-'" + this._sessionStateName + "'";
      
      if (mainHAPartitionName == null)
         haPartitionName = DEFAULT_PARTITION_JNDI_NAME;
      else
         haPartitionName = mainHAPartitionName;
      
      haPartitionJndiName = JNDI_FOLDER_NAME_FOR_HAPARTITION + haPartitionName;
      
      if (beanCleaningDelay > 0)
         this.beanCleaningDelay = beanCleaningDelay;
      else
         this.beanCleaningDelay = MAX_DELAY_BEFORE_CLEANING_UNRECLAIMED_STATE;
      
   }
   
   public void init () throws Exception
   {
      this.log = Logger.getLogger(HASessionStateImpl.class.getName() + "." + this._sessionStateName);
      
      // let's first find the HAPartition
      //
      Context ctx = new InitialContext ();
      
      this.hapGeneral = (HAPartition)ctx.lookup (haPartitionJndiName);
      
      if (hapGeneral == null)
         log.error ("Unable to get default HAPartition under name '" + haPartitionJndiName + "'.");
      
      this.hapGeneral.registerRPCHandler (this.sessionStateIdentifier, this);
      this.hapGeneral.subscribeToStateTransferEvents (this.HA_SESSION_STATE_STATE_TRANSFER, this);
      this.bind (this._sessionStateName, this, HASessionStateImpl.class, ctx);
   }
   
   protected void bind (String jndiName, Object who, Class classType, Context ctx) throws Exception
   {
      // Ah ! This service isn't serializable, so we use a helper class
      //
      NonSerializableFactory.bind (jndiName, who);
      Name n = ctx.getNameParser ("").parse (jndiName);
      while (n.size () > 1)
      {
         String ctxName = n.get (0);
         try
         {
            ctx = (Context)ctx.lookup (ctxName);
         }
         catch (NameNotFoundException e)
         {
            log.debug ("creating Subcontext" + ctxName);
            ctx = ctx.createSubcontext (ctxName);
         }
         n = n.getSuffix (1);
      }
      
      // The helper class NonSerializableFactory uses address type nns, we go on to
      // use the helper class to bind the service object in JNDI
      //
      StringRefAddr addr = new StringRefAddr ("nns", jndiName);
      Reference ref = new Reference ( classType.getName (), addr, NonSerializableFactory.class.getName (), null);
      ctx.bind (n.get (0), ref);
   }
   
   public void start () throws Exception
   {
      this.myNodeName = this.hapGeneral.getNodeName ();
      log.debug ("HASessionState node name : " + this.myNodeName );
   }
   
   public void stop () throws Exception
   {
      try
      {
         Context ctx = new InitialContext ();         
         ctx.unbind (this._sessionStateName);
         NonSerializableFactory.unbind (this._sessionStateName);
      }
      catch (Exception ignored)
      {}
   }
   
   public String getNodeName ()
   {
      return this.myNodeName ;
   }
   
   // Used for Session state transfer
   //
   public Serializable getCurrentState ()
   {
      log.debug ("Building and returning state of HASessionState");
      
      if (this.appSessions == null)
         this.appSessions = new Hashtable ();
      
      Serializable result = null;
      
      synchronized (this.lockAppSession)
      {
         this.purgeState ();
         
         try
         {
            result = deflate (this.appSessions);
         }
         catch (Exception e)
         {
            log.error("operation failed", e);
         }
      }
      return result;
   }
   
   public void setCurrentState (Serializable newState)
   {
      log.debug ("Receiving state of HASessionState");
      
      if (this.appSessions == null)
         this.appSessions = new Hashtable ();
      
      synchronized (this.lockAppSession)
      {
         try
         {
            this.appSessions.clear (); // hope to facilitate the job of the GC
            this.appSessions = (Hashtable)inflate ((byte[])newState);
         }
         catch (Exception e)
         {
            log.error("operation failed", e);
         }         
      }
   }
   
   public void purgeState ()
   {
      synchronized (this.lockAppSession)
      {
         for (Enumeration keyEnum = this.appSessions.keys (); keyEnum.hasMoreElements ();)
         {
            // trip in apps..
            //
            Object key = keyEnum.nextElement ();
            Hashtable value = (Hashtable)this.appSessions.get (key);
            long currentTime = System.currentTimeMillis ();
            
            for (Iterator iterSessions = value.values ().iterator (); iterSessions.hasNext ();)
            {
               PackagedSession ps = (PackagedSession)iterSessions.next ();
               if ( (currentTime - ps.unmodifiedExistenceInVM ()) > beanCleaningDelay )
                  iterSessions.remove ();
            }
         }
      }
      
   }
   
   protected byte[] deflate (Object object) throws IOException
   {
      ByteArrayOutputStream baos = new ByteArrayOutputStream ();
      Deflater def = new Deflater (java.util.zip.Deflater.BEST_COMPRESSION);
      DeflaterOutputStream dos = new DeflaterOutputStream (baos, def);
      
      ObjectOutputStream out = new ObjectOutputStream (dos);
      out.writeObject (object);
      out.close ();
      dos.finish ();
      dos.close ();
      
      return baos.toByteArray ();
   }
   
   protected Object inflate (byte[] compressedContent) throws IOException
   {
      if (compressedContent==null)
         return null;
      
      try
      {
         ObjectInputStream in = new ObjectInputStream (new InflaterInputStream (new ByteArrayInputStream (compressedContent)));
         
         Object object = in.readObject ();
         in.close ();
         return object;
      }
      catch (Exception e)
      {
         throw new IOException (e.toString ());
      }
   }
   
   protected Hashtable appSessions = new Hashtable ();
   protected Object lockAppSession = new Object ();
   
   protected Hashtable getHashtableForApp (String appName)
   {
      if (this.appSessions == null)
         this.appSessions = new Hashtable (); // should never happen though...
      
      Hashtable result = null;
      
      synchronized (this.lockAppSession)
      {
         result = (Hashtable)this.appSessions.get (appName);
         if (result == null)
         {
            result = new Hashtable ();
            this.appSessions.put (appName, result);
         }
      }
      return result;
   }
   
   public void createSession (String appName, Object keyId)
   {
      this._createSession (appName, keyId);
   }
   
   public PackagedSessionImpl _createSession (String appName, Object keyId)
   {
      Hashtable app = this.getHashtableForApp (appName);
      PackagedSessionImpl result = new PackagedSessionImpl ((Serializable)keyId, null, this.myNodeName);
      app.put (keyId, result);
      return result;
   }
   
   public void setState (String appName, Object keyId, byte[] state)
      throws java.rmi.RemoteException
   {
      Hashtable app = this.getHashtableForApp (appName);
      PackagedSession ps = (PackagedSession)app.get (keyId);
      
      if (ps == null)
      {
         ps = _createSession (appName, keyId);
      }
            
      boolean isStateIdentical = false;
      
      Mutex mtx = getLock (appName, keyId);
      try { 
         if (!mtx.attempt (0))
            throw new java.rmi.RemoteException ("Concurent calls on session object.");
      } 
      catch (InterruptedException ie) { log.info (ie); return; }
      
      try
      {
         isStateIdentical = ps.setState(state);
         if (!isStateIdentical)
         {
            Object[] args =
               {appName, ps};
            try
            {
               this.hapGeneral.callMethodOnCluster (this.sessionStateIdentifier, "_setState", args, true);
            }
            catch (Exception e)
            {
               log.error("operation failed", e);
            }
         }
      }
      finally
      {
         mtx.release ();
      }
   }
   
   /*
   public void _setStates (String appName, Hashtable packagedSessions)
   {
      synchronized (this.lockAppSession)
      {
         Hashtable app = this.getHashtableForApp (appName);
         
         if (app == null)
         {
            app = new Hashtable (packagedSessions.size ());
            this.appSessions.put (appName, app);
         }
         app.putAll (packagedSessions);
      }
   }*/
   
   public void _setState (String appName, PackagedSession session)
   {
      Hashtable app = this.getHashtableForApp (appName);
      PackagedSession ps = (PackagedSession)app.get (session.getKey ());
      
      if (ps == null)
      {
         ps = session;
         synchronized (app)
         {
            app.put (ps.getKey (), ps);
         }
      }
      else
      {
         Mutex mtx = getLock (appName, session.getKey ());
         try { mtx.acquire (); } catch (InterruptedException ie) { log.info (ie); return; }
         
         try
         {
            if (ps.getOwner ().equals (this.myNodeName))
            {
               // a modification has occured externally while we were the owner
               //
               ownedObjectExternallyModified (appName, session.getKey (), ps, session);
            }
            ps.update (session);
         }
         finally
         {
            mtx.release ();
         }
      }
      
   }
   
   public PackagedSession getState (String appName, Object keyId)
   {
      Hashtable app = this.getHashtableForApp (appName);
      return (PackagedSession)app.get (keyId);
   }
   
   public PackagedSession getStateWithOwnership (String appName, Object keyId) throws java.rmi.RemoteException
   {
      return this.localTakeOwnership (appName, keyId);
   }
   
   public PackagedSession localTakeOwnership (String appName, Object keyId) throws java.rmi.RemoteException
   {
      Hashtable app = this.getHashtableForApp (appName);
      PackagedSession ps = (PackagedSession)app.get (keyId);
      
      // if the session is not yet available, we simply return null. The persistence manager
      // will have to take an action accordingly
      //
      if (ps == null)
         return null;
      
      Mutex mtx = getLock (appName, keyId);
      
      try { 
         if (!mtx.attempt (0))
            throw new java.rmi.RemoteException ("Concurent calls on session object.");
      } 
      catch (InterruptedException ie) { log.info (ie); return null; }
      
      try
      {
         if (!ps.getOwner ().equals (this.myNodeName))
         {
            Object[] args =
            {appName, keyId, this.myNodeName, new Long (ps.getVersion ())};
            ArrayList answers = null;
            try
            {
               answers = this.hapGeneral.callMethodOnCluster (this.sessionStateIdentifier, "_setOwnership", args, true);
            }
            catch (Exception e)
            {
               log.error("operation failed", e);
            }
            
            if (answers != null && answers.contains (Boolean.FALSE))
               throw new java.rmi.RemoteException ("Concurent calls on session object.");
            else
            {
               ps.setOwner (this.myNodeName);
               return ps;
            }
         }
         else
            return ps;
      }
      finally
      {
         mtx.release ();
      }
   }
   
   public Boolean _setOwnership (String appName, Object keyId, String newOwner, Long remoteVersion)
   {
      Hashtable app = this.getHashtableForApp (appName);
      PackagedSession ps = (PackagedSession)app.get (keyId);
      Boolean answer = Boolean.TRUE;
      Mutex mtx = getLock (appName, keyId);
      
      try { 
         if (!mtx.attempt (0))
            return Boolean.FALSE;
      } 
      catch (InterruptedException ie) { log.info (ie); return Boolean.FALSE; }

      try
      {
         if (!ps.getOwner ().equals (this.myNodeName))
         {
            // this is not our business... we don't care
            // we do not update the owner of ps as another host may refuse the _setOwnership call
            // anyway, the update will be sent to us later if state is modified
            //
            //ps.setOwner (newOwner);
            answer = Boolean.TRUE;
         }
         else if (ps.getVersion () > remoteVersion.longValue ())
         {
            // we are concerned and our version is more recent than the one of the remote host!
            // it means that we have concurrent calls on the same state that has not yet been updated
            // this means we will need to raise a java.rmi.RemoteException
            //
            answer = Boolean.FALSE;
         }
         else
         {
            // the remote host has the same version as us (or more recent? possible?)
            // we need to update the ownership. We can do this because we know that no other
            // node can refuse the _setOwnership call
            ps.setOwner (newOwner);
            ownedObjectExternallyModified (appName, keyId, ps, ps);
            answer = Boolean.TRUE;
         }
      }
      finally
      {
         mtx.release ();
      }
      return answer;
   }
   
   public void takeOwnership (String appName, Object keyId) throws java.rmi.RemoteException
   {
      this.localTakeOwnership (appName, keyId);
   }
   
   public void removeSession (String appName, Object keyId)
   {
      Hashtable app = this.getHashtableForApp (appName);
      if (app != null)
      {
         PackagedSession ps = (PackagedSession)app.remove (keyId);
         if (ps != null)
         {
            removeLock (appName, keyId);
            Object[] args =
               { appName, keyId };
            try
            {
               this.hapGeneral.callMethodOnCluster (this.sessionStateIdentifier, "_removeSession", args, true);
            }
            catch (Exception e)
            { log.error("operation failed", e); }
         }
      }
   }
   
   public void _removeSession (String appName, Object keyId)
   {
      Hashtable app = this.getHashtableForApp (appName);
      PackagedSession ps = null;
      ps = (PackagedSession)app.remove (keyId);
      if (ps != null && ps.getOwner ().equals (this.myNodeName))
         ownedObjectExternallyModified (appName, keyId, ps, ps);
      
      removeLock (appName, keyId);
   }
   
   protected Hashtable listeners = new Hashtable ();
   
   public synchronized void subscribe (String appName, HASessionStateListener listener)
   {
      Vector members = (Vector)listeners.get (appName);
      if (members == null)
      {
         members = new Vector ();
         listeners.put (appName, members);
      }
      if (!members.contains (listener))
      {
         members.add (listener);
      }

   }
   
   public synchronized void unsubscribe (String appName, HASessionStateListener listener)
   {
      Vector members = (Vector)listeners.get (appName);
      if ((members != null) && members.contains (listener))
         members.remove (listener);
   }
   
   public void ownedObjectExternallyModified (String appName, Object key, PackagedSession oldSession, PackagedSession newSession)
   {
      Vector members = (Vector)listeners.get (appName);
      if (members != null)
         for (int i=0; i<members.size (); i++)
         try
         {
            ((HASessionStateListener)members.elementAt (i)).sessionExternallyModified (newSession);
         }
         catch (Throwable t)
         {
            log.debug (t);
         }
   }
   
   public HAPartition getCurrentHAPartition ()
   {
      return this.hapGeneral;
   }
   
   
   protected boolean lockExists (String appName, Object key)
   {
      synchronized (this.locks)
      {
         HashMap ls = (HashMap)this.locks.get (appName);
         if (ls == null)
            return false;
         
         return (ls.get(key)!=null);
      }
   }

   protected Mutex getLock (String appName, Object key)
   {
      synchronized (this.locks)
      {
         HashMap ls = (HashMap)this.locks.get (appName);
         if (ls == null)
         {
            ls = new HashMap ();
            this.locks.put (appName, ls);
         }
          
         Mutex mutex = (Mutex)ls.get(key);
         if (mutex == null)
         {
            mutex = new Mutex ();
            ls.put (key, mutex);
         }
         
         return mutex;         
      }
   }

   protected void removeLock (String appName, Object key)
   {
      synchronized (this.locks)
      {
         HashMap ls = (HashMap)this.locks.get (appName);
         if (ls == null)
            return;
         ls.remove (key);
      }
   }
   
}
