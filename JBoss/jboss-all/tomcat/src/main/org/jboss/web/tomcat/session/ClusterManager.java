/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.session;

import org.apache.catalina.LifecycleException;
import org.apache.catalina.Session;
import org.apache.catalina.session.StandardManager;
import org.apache.catalina.Context;
import org.apache.catalina.Globals;
import org.apache.catalina.Host;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.logging.Logger;
import org.jboss.ha.httpsession.server.ClusteredHTTPSessionServiceMBean;
import org.jboss.metadata.WebMetaData;

import java.io.IOException;
import javax.ejb.EJBException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.Cookie;

/** Implementation of a clustered session manager for
 *  catalina.
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>23.11.2003 Sacha Labourey:</b>
 * <ul>
 * <li> Avoid unnecessary session replication (3 policies)</li>
 * </ul>

 @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionService

 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.1.1.2.2 $
 */
public class ClusterManager extends StandardManager implements ClusterManagerMBean
{
   // -- Constants ----------------------------------------

   /**
    * Informational name for this Catalina component
    */
   private static final String info = "ClusterManager/1.0";

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
    * The Log-object for this class
    */
   private Logger log;

   /**
    * The session-reaper thread
    */
   protected ClusteredSessionCleanup cleanupThread = null;

   /**
    * The context-path this Manager is configured for
    */
   protected String contextPath;

   /**
    * The host-name this Manager is configured for
    */
   protected String hostName;

   /**
    * The objectname this Manager is associated with
    */
   protected ObjectName objectName;

   /**
    * The context for this Manager
    */
   protected Context context;

   /**
    * The host for this Manager
    */
   protected Host host;

   /**
    * Is the reaper-thread started?
    */
   protected boolean started = false;

   protected int invalidateSessionPolicy = WebMetaData.SESSION_INVALIDATE_SET_AND_NON_PRIMITIVE_GET;
   protected int replicationType = WebMetaData.REPLICATION_TYPE_SYNC;

   public ClusterManager(Host host, Context context, Logger log, WebMetaData webMetaData) throws ClusteringNotSupportedException
   {
      super();

      // We only allow serializable session attributes
      setDistributable(true);
      this.log = log;
      contextPath = context.getPath();
      this.context = context;
      hostName = host.getName();
      this.host = host;
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
         if (contextPath.equals(""))
         {
            contextPath = "/";
         }
         objectName = new ObjectName("jboss.web:service=ClusterManager,context=" + contextPath + ",host=" + hostName);

         log.info("ClusteredHTTPSessionService found");
      }
      catch (Throwable e)
      {
         log.error("Could not create ObjectName", e);
         throw new ClusteringNotSupportedException(e.toString());
      }
      // Create a cleanupThread-object
      cleanupThread = new ClusteredSessionCleanup(this, log);
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


   // Manager-methods -------------------------------------

   /**
    * Create a new session
    */
   public Session createSession()
   {
      ClusteredSession session = new ClusteredSession(this, log);

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
         if (context.getCookies())
         {
            // set a new session cookie
            Cookie newCookie = new Cookie(Globals.SESSION_COOKIE_NAME, sessionId);
            log.debug("Setting cookie with session id:" + sessionId + " & name:" + Globals.SESSION_COOKIE_NAME);
            newCookie.setMaxAge(-1);
            newCookie.setPath(contextPath);
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

      HttpServletResponse response = (HttpServletResponse) ClusteredSessionValve.responseThreadLocal.get();

      synchronized (sessions)
      {

         // first in local store
         session = (ClusteredSession) sessions.get(id);
      }

      if (session == null)
      {
         String key = getJvmRouteId(id);
         synchronized (sessions)
         {
            //check for sessionid with new jvmRoute because of session failover
            session = (ClusteredSession) sessions.get(key);
         }

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
            session.initAfterLoad(this, log);

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
         String key = session.getId();

         synchronized (sessions)
         {
            // add to local store first
            sessions.put(key, session);
         }
         try
         {
            // add to distributed store
            storeSession((ClusteredSession) session);
         }
         catch (Exception e)
         {
            log.error("Adding a session to the clustered store failed", e);
         }
         if (log.isDebugEnabled())
            log.debug("Session with id=" + key + " added");
      }
      else
      {
         throw new IllegalArgumentException("You can only add ClusteredSessions to this Manager");
      }
   }

   /**
    * Removes a session from this Manager
    * @param session The session that wants to be removed
    */
   public void remove(Session session)
   {
      if (session == null)
      {
         return;
      }
      String key = session.getId();

      synchronized (sessions)
      {
         // remove from local store
         sessions.remove(key);
      }

      try
      {
         // remove from distributed store
         if (session instanceof ClusteredSession)
         {
            ClusteredSession s = (ClusteredSession)session;
            if (!s.isReplicationTypeAlreadySet())
               s.setReplicationTypeForSession(WebMetaData.REPLICATION_TYPE_ASYNC); //use async by default
         }
         removeSession(key);
      }
      catch (Exception e)
      {
         log.warn("Removing a session from the clustered store failed", e);
      }
      if (log.isDebugEnabled())
         log.debug("Session with id=" + key + " removed");

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

   public int getInvalidateSessionPolicy()
   {
      return this.invalidateSessionPolicy;
   }

   public int getReplicationType()
   {
      return replicationType;
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

      // Start the background reaper thread
      cleanupThread.threadStart();

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

      // Stop the background reaper thread
      cleanupThread.threadStop();

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

         if (!session.isReplicationTypeAlreadySet())
            session.setReplicationTypeForSession(this.replicationType); //set default if not yet overidden
         proxy.setHttpSession(session.getId(), session);
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
}
