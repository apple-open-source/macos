/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc5.session;

import org.apache.catalina.LifecycleException;
import org.apache.catalina.Session;
import org.apache.catalina.session.StandardManager;
import org.apache.catalina.Context;
import org.apache.catalina.Globals;
import org.apache.catalina.Host;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;
import org.jboss.ha.httpsession.server.ClusteredHTTPSessionServiceMBean;
import java.io.IOException;
import javax.ejb.EJBException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.Cookie;

import org.jboss.logging.Logger;
import org.jboss.metadata.WebMetaData;

/** Implementation of a clustered session manager for
 *  catalina.

 @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionService

 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.2.1 $
 */
public class JBossManager 
    extends StandardManager implements JBossManagerMBean
{

   // -- Constants ----------------------------------------

   /**
    * Informational name for this Catalina component
    */
   private static final String info = "JBossManager/1.0";

   // -- Class attributes ---------------------------------

   /**
    * Proxy-object for the ClusteredHTTPSessionService
    */
   private ClusteredHTTPSessionServiceMBean proxy;

   /**
    * The ObjectName for the ClusteredHttpSessionService
    */
   private ObjectName clusteredHttpServiceName;

   /**
    * The objectname this Manager is associated with
    */
   protected ObjectName objectName;

   /**
    * Is the reaper-thread started?
    */
   protected boolean started = false;

   /**
    * The Log-object for this class
    */
   private Logger log;

   protected int invalidateSessionPolicy = WebMetaData.SESSION_INVALIDATE_SET_AND_NON_PRIMITIVE_GET;
   protected int replicationType = WebMetaData.REPLICATION_TYPE_SYNC;

   public JBossManager(String name, Logger log, WebMetaData webMetaData) 
      throws ClusteringNotSupportedException
   {
      super();

      this.log = log;
      // We only allow serializable session attributes
      setDistributable(true);
      this.invalidateSessionPolicy = webMetaData.getInvalidateSessionPolicy();
      this.replicationType = webMetaData.getReplicationType();

      // Find ClusteredHttpSessionService
      try
      {
         clusteredHttpServiceName = new ObjectName("jboss", "service", "ClusteredHttpSession");
         // Create Proxy-Object for this service
         proxy = (ClusteredHTTPSessionServiceMBean) MBeanProxyExt.create(ClusteredHTTPSessionServiceMBean.class, clusteredHttpServiceName);
      }
      catch (Throwable e)
      {
         log.info("ClusteredHTTPSessionService not found");
         throw new ClusteringNotSupportedException("ClusteredHTTPSessionService not found");
      }

      try
      {
         // set the JBoss' ClusteredHttpSession service timeout to 4 hours because we have our own expiry mechanism
         proxy.setSessionTimeout(14400000);

         // Give this manager a name
         objectName = new ObjectName("jboss.web:service=ClusterManager,WebModule=" + name);

         log.info("ClusteredHTTPSessionService found");
      }
      catch (Throwable e)
      {
         log.error("Could not create ObjectName", e);
         throw new ClusteringNotSupportedException(e.toString());
      }
   }

   // MBean-methods ---------------------------------------
   public Integer getLocalActiveSessionCount()
   {
      return new Integer(sessions.size());
   }

   public ClusteredSession[] getSessions()
   {
      ClusteredSession[] sess = new ClusteredSession[0];

      synchronized (sessions)
      {
         sess = (ClusteredSession[]) sessions.values().toArray(sess);
      }
      return sess;
   }

   public int getInvalidateSessionPolicy()
   {
      return this.invalidateSessionPolicy;
   }

   public int getReplicationType()
   {
      return replicationType;
   }


   // Manager-methods -------------------------------------

   /**
    * Create a new session
    */
   public Session createSession()
   {
      ClusteredSession session = new ClusteredSession(this);

      session.setNew(true);
      session.setCreationTime(System.currentTimeMillis());
      session.setMaxInactiveInterval(this.maxInactiveInterval);

      String sessionId = this.getNextId();
      String jvmRoute = this.getJvmRoute();
      if (jvmRoute != null)
      {
         sessionId += '.' + jvmRoute;
      }
      session.setId(sessionId);

      session.setValid(true);

      return session;
   }

   /**
    * Generate new sessionid for a new jvmRoute - during failover
    * @param id The session id
    */
   public String getJvmRouteId(String id)
   {
      String sessid = null;
      if (id != null)
      {
         if (this.getJvmRoute() != null)
         {
            if (!this.getJvmRoute().equals(id.substring(id.indexOf('.') + 1, id.length())))
            {
               sessid = id.substring(0, id.indexOf('.') + 1) + this.getJvmRoute();
               log.debug("JvmRoute id is :" + sessid);
            }
            else
            {
               return id;
            }
         }
      }
      return sessid;
   }

   /**
    * Sets a new cookie for the given session id and response
    * @param response The HttpServletResponse to which cookie is to be added
    * @param sessionId The session id
    */
   public void setSessionCookie(HttpServletResponse response, String sessionId)
   {
      if (response != null)
      {
          Context context = (Context) container;
         if (context.getCookies())
         {
            // set a new session cookie
            Cookie newCookie = new Cookie(Globals.SESSION_COOKIE_NAME, sessionId);
            log.debug("Setting cookie with session id:" + sessionId + " & name:" + Globals.SESSION_COOKIE_NAME);
            newCookie.setMaxAge(-1);
            if (context.getPath().equals("")) {
                newCookie.setPath("/");
            } else {
                newCookie.setPath(context.getPath());
            }
            response.addCookie(newCookie);
         }
      }
   }

   /**
    * Find the session for the given id
    * @param id The session id
    * @return The session for the given id or null if not found in local or distributed store
    */
   public Session findSession(String id) throws IOException
   {
      ClusteredSession session = null;

      if (id == null)
      {
         return null;
      }

      log.debug("Looking for session with id=" + id);

      synchronized (sessions)
      {
         HttpServletResponse response = (HttpServletResponse) ClusteredSessionValve.responseThreadLocal.get();

         // first in local store
         session = (ClusteredSession) sessions.get(id);

         if (session == null)
         {
            //check for sessionid with new jvmRoute because of session failover
            session = (ClusteredSession) sessions.get(getJvmRouteId(id));

            //set cookie with new sessionid
            if (session != null)
            {
               // Do we use Cookies for session id storage?
               setSessionCookie(response, session.getId());
            }
         }

         // not found --> distributed store
         if (session == null)
         {
            session = loadSession(id);

            if (session == null)
            {
               session = loadSession(getJvmRouteId(id));
            }
            // did we find the session in the distributed store?
            if (session != null)
            {
               // set attributes that were not serialized (are marked transient)
               session.initAfterLoad(this);

               // If jvmRoute is set manipulate the sessionid and generate a cookie to make
               // the session sticky on its new node

               if (this.getJvmRoute() != null)
               {
                  String sessionid = getJvmRouteId(id);

                  //setId() resets session id and adds it back to local & distributed store
                  session.setId(sessionid);

                  //set cookie (if using cookies for session)
                  setSessionCookie(response, sessionid);

               }
               else
               {
                  // add to local store - no jvmRoute specified
                  log.debug("Found in distributed store - adding to local store");
                  add(session);
               }
            }
         }
      }

      if (session != null)
      {
         log.debug("Found");
      }
      return session;
   }

   /**
    * Add session to this Manager
    * @param session The session that wants to be added
    */
   public void add(Session session)
   {
      if (session == null)
      {
         return;
      }
      // is this session of the right type?
      if (session instanceof ClusteredSession)
      {
         synchronized (sessions)
         {
            // add to local store first
            sessions.put(session.getId(), session);

            try
            {
               // add to distributed store
               storeSession((ClusteredSession) session);
            }
            catch (Exception e)
            {
               log.error("Adding a session to the clustered store failed", e);
            }
            log.debug("Session with id=" + session.getId() + " added");
         }
      }
      else
      {
         throw new IllegalArgumentException("You can only add ClusteredSessions to this Manager");
      }
   }

   /**
    * Removes a session from this Manager
    * @param The session that wants to be removed
    */
   public void remove(Session session)
   {
      if (session == null)
      {
         return;
      }
      synchronized (sessions)
      {
         try
         {
            // remove from distributed store
            removeSession(session.getId());
         }
         catch (Exception e)
         {
            log.warn("Removing a session from the clustered store failed", e);
         }
         // remove from local store
         sessions.remove(session.getId());
         log.debug("Session with id=" + session.getId() + " removed");
      }
   }

   /**
    * Remove a session from the local store only
    * @param session the session to be removed
    */
   protected void removeLocal(Session session)
   {
      if (session == null)
      {
         return;
      }
      synchronized (sessions)
      {
         sessions.remove(session.getId());
      }
   }

   /**
    * Remove a session from the local store only
    * @param id the session id of the session to be removed
    */
   public void removeLocal(String id)
   {
      if (id == null)
      {
         return;
      }
      synchronized (sessions)
      {
         sessions.remove(id);
      }
   }


   protected void recycle(Session session)
   {
      // ignore - we do no recycling
   }

   /**
    * Get a informational string about this class
    * @return Information string
    */
   public String getInfo()
   {
      return info;
   }

   /**
    * Start this Manager
    * @throws LifecycleException
    */
   public void start() throws LifecycleException
   {
      startManager();
   }

   /**
    * Stop this Manager
    * @throws LifecycleException
    */
   public void stop() throws LifecycleException
   {
      stopManager();
   }

   /**
    * Prepare for the beginning of active use of the public methods of this
    * component.  This method should be called after <code>configure()</code>,
    * and before any of the public methods of the component are utilized.
    *
    * @exception IllegalStateException if this component has already been
    *  started
    * @exception LifecycleException if this component detects a fatal error
    *  that prevents this component from being used
    */
   protected void startManager() throws LifecycleException
   {
      if (debug >= 1)
         log.info("Starting");

      // Validate and update our current component state
      if (started)
         throw new LifecycleException
               (sm.getString("standardManager.alreadyStarted"));
      lifecycle.fireLifecycleEvent(START_EVENT, null);
      started = true;

      // register ClusterManagerMBean to the MBeanServer
      try
      {
         MBeanServer server = MBeanServerLocator.locateJBoss();
         server.registerMBean(this, objectName);
      }
      catch (Exception e)
      {
         log.error("Could not register ClusterManagerMBean to MBeanServer", e);
      }
   }

   /**
    * Gracefully terminate the active use of the public methods of this
    * component.  This method should be the last one called on a given
    * instance of this component.
    *
    * @exception IllegalStateException if this component has not been started
    * @exception LifecycleException if this component detects a fatal error
    *  that needs to be reported
    */
   protected void stopManager() throws LifecycleException
   {
      if (debug >= 1)
         log.info("Stopping");

      // Validate and update our current component state
      if (!started)
         throw new LifecycleException
               (sm.getString("standardManager.notStarted"));
      lifecycle.fireLifecycleEvent(STOP_EVENT, null);
      started = false;

      // unregister ClusterManagerMBean from the MBeanServer
      try
      {
         MBeanServer server = MBeanServerLocator.locateJBoss();
         server.unregisterMBean(objectName);
      }
      catch (Exception e)
      {
         log.error("Could not unregister ClusterManagerMBean from MBeanServer", e);
      }
   }

   /**
    * Load persisted sessions (NOT supported by this Manager)
    */
   public void load() throws ClassNotFoundException, IOException
   {
      // We do not support persistence for sessions
   }

   /**
    * Load persisted sessions (NOT supported by this Manager)
    */
   public void unload() throws IOException
   {
      // We do not support persistence for sessions
   }

   /**
    * Overloaded run()-method of the session-cleanup-thread.
    * We have our own cleanup-code - so no code here
    */
   public void run()
   {
      // We do our own expire() so no code here
   }
   // private methods ----------------------------------------

   /**
    * Get a new session-id from the distributed store
    * @return new session-id
    */
   private String getNextId()
   {
      return proxy.getSessionId();
   }

   /**
    * Store a session in the distributed store
    * @param session The session to store
    */
   protected void storeSession(ClusteredSession session)
   {
      if (session == null)
      {
         return;
      }
      if (session.isValid())
      {
         // Notify all session attributes that they get serialized (SRV 7.7.2)
	 session.passivate();

	 if(log.isDebugEnabled()) {
	    log.debug("Replicating session with id "+session.getId());
	 }

         proxy.setHttpSession(session.getId(), (SerializableHttpSession) session);
      }
   }

   /**
    * Load a session from the distributed store
    * @param id The session-id for the session to load
    * @return the session or null if the session cannot be found in the distributed store
    */
   protected ClusteredSession loadSession(String id)
   {
      ClusteredSession session = null;

      if (id == null)
      {
         return null;
      }

      try
      {
         /* Pass in the web ctx class loader to handle the loading of classes
            that originate from the web application war.
         */
         ClassLoader ctxCL = super.getContainer().getLoader().getClassLoader();
         session = (ClusteredSession) proxy.getHttpSession(id, ctxCL);
      }
      catch (EJBException e)
      {
         // ignore
         log.debug("Loading a session out of the clustered store failed", e);
      }

      return session;
   }

   /**
    * Remove a session from the distributed store
    * @param id The session-id for the session to remove
    */
   protected void removeSession(String id)
   {
      if (id == null)
      {
         return;
      }
      try
      {
         proxy.removeHttpSession(id);
      }
      catch (EJBException e)
      {
         //ignore
         log.debug("Removing a session out of the clustered store failed", e);
      }
   }

   /**
    * Go through all sessions and look if they have expired
    */
   public void processExpires()
   {
      // What's the time?
      long timeNow = System.currentTimeMillis();

      // Get all sessions
      Session sessions[] = findSessions();

      log.debug("Looking for sessions that have expired");

      for (int i = 0; i < sessions.length; ++i)
      {
         ClusteredSession session = (ClusteredSession) sessions[i];

         // We only look at valid sessions
         if (!session.isValid())
         {
            continue;
         }

         // How long are they allowed to be idle?
         int maxInactiveInterval = session.getMaxInactiveInterval();

         // Negative values = never expire
         if (maxInactiveInterval < 0)
         {
            continue;
         }

         // How long has this session been idle?
         int timeIdle =
               (int) ((timeNow - session.getLastAccessedTime()) / 1000L);

         // Too long?
         if (timeIdle >= maxInactiveInterval)
         {
            try
            {
               log.debug("Session with id = " + session.getId() + " has expired on local node");
               // Did another node access this session?
               // Try to get the session from the clustered store
               ClusteredSession clusteredSession = loadSession(session.getId());
               if (clusteredSession != null)
               {
                  int timeIdleCluster =
                        (int) ((timeNow - clusteredSession.getLastAccessedTime()) / 1000L);
                  if (timeIdleCluster < maxInactiveInterval)
                  {
                     log.debug("Session " + session.getId() + " has only expired on local node but is alive on another node - removing only from local store");
                     // Remove from local store, because the session is
                     // alive on another node
                     removeLocal(session);
                     continue;
                  }

                  log.debug("Session " + session.getId() + " has also expired on all other nodes - removing globally");
               }


               // Kick this session
               session.expire();
            }
            catch (Throwable t)
            {
               log.error("Problems while expiring session with id = " + session.getId(), t);
            }
         }
      }
   }


}
