/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.session;

import org.apache.catalina.Manager;
import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;
import java.beans.PropertyChangeSupport;
import java.io.Serializable;
import java.io.IOException;
import java.security.Principal;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import javax.servlet.ServletContext;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionActivationListener;
import javax.servlet.http.HttpSessionAttributeListener;
import javax.servlet.http.HttpSessionBindingEvent;
import javax.servlet.http.HttpSessionBindingListener;
import javax.servlet.http.HttpSessionContext;
import javax.servlet.http.HttpSessionEvent;
import javax.servlet.http.HttpSessionListener;
import org.apache.catalina.Context;
import org.apache.catalina.Manager;
import org.apache.catalina.Session;
import org.apache.catalina.SessionEvent;
import org.apache.catalina.SessionListener;
import org.apache.catalina.core.StandardContext;
import org.apache.catalina.util.Enumerator;
import org.apache.catalina.util.StringManager;
import org.apache.catalina.session.StandardSessionFacade;
import org.jboss.logging.Logger;
import org.jboss.metadata.WebMetaData;

/** Implementation of a clustered session for the ClusteredHttpSession-Service
 *  @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionService
 *
 *  It is based on the standard catalina class org.apache.catalina.session.StandardSession
 *  (Revision: 1.25.2.1 Release: 4.0.3)
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>23.11.2003 Sacha Labourey:</b>
 * <ul>
 * <li> Avoid unnecessary session replication (3 policies)</li>
 * </ul>

 @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionService

 @author Thomas Peuss <jboss@peuss.de>
 @author Sacha Labourey <sacha.labourey@jboss.org>
 @version $Revision: 1.1.1.1.2.3 $
 */
class ClusteredSession
      implements SerializableHttpSession, HttpSession, Session, Serializable
{
   private static final long serialVersionUID = -758573655613558722L;

   // ----------------------------------------------------- Instance Variables


   /**
    * The collection of user data attributes associated with this Session.
    */
   private HashMap attributes = new HashMap();


   /**
    * The authentication type used to authenticate our cached Principal,
    * if any.  NOTE:  This value is not included in the serialized
    * version of this object.
    */
   private transient String authType = null;


   /**
    * The time this session was created, in milliseconds since midnight,
    * January 1, 1970 GMT.
    */
   private long creationTime = 0L;


   /**
    * The debugging detail level for this component.  NOTE:  This value
    * is not included in the serialized version of this object.
    */
   private transient int debug = 0;


   /**
    * We are currently processing a session expiration, so bypass
    * certain IllegalStateException tests.  NOTE:  This value is not
    * included in the serialized version of this object.
    */
   private transient boolean expiring = false;


   /**
    * The facade associated with this session.  NOTE:  This value is not
    * included in the serialized version of this object.
    */
   private transient StandardSessionFacade facade = null;


   /**
    * The session identifier of this Session.
    */
   private String id = null;


   /**
    * Descriptive information describing this Session implementation.
    */
   private static final String info = "ClusteredSession/1.0";


   /**
    * The last accessed time for this Session.
    */
   private long lastAccessedTime = creationTime;


   /**
    * The session event listeners for this Session.
    */
   private transient ArrayList listeners = new ArrayList();


   /**
    * The Manager with which this Session is associated.
    */
   private transient Manager manager = null;


   /**
    * The maximum time interval, in seconds, between client requests before
    * the servlet container may invalidate this session.  A negative time
    * indicates that the session should never time out.
    */
   private int maxInactiveInterval = -1;


   /**
    * Flag indicating whether this session is new or not.
    */
   private boolean isNew = false;


   /**
    * Flag indicating whether this session is valid or not.
    */
   private boolean isValid = false;


   /**
    * Internal notes associated with this session by Catalina components
    * and event listeners.  <b>IMPLEMENTATION NOTE:</b> This object is
    * <em>not</em> saved and restored across session serializations!
    */
   private transient HashMap notes = new HashMap();


   /**
    * The authenticated Principal associated with this session, if any.
    * <b>IMPLEMENTATION NOTE:</b>  This object is <i>not</i> saved and
    * restored across session serializations!
    */
   private transient Principal principal = null;


   /**
    * The string manager for this package.
    */
   private static StringManager sm =
         StringManager.getManager("org.jboss.web.tomcat.session");

   /**
    * The HTTP session context associated with this session.
    */
   private static HttpSessionContext sessionContext = null;


   /**
    * The property change support for this component.  NOTE:  This value
    * is not included in the serialized version of this object.
    */
   private transient PropertyChangeSupport support =
         new PropertyChangeSupport(this);


   /**
    * The current accessed time for this session.
    */
   private long thisAccessedTime = creationTime;

   protected transient boolean isSessionModifiedSinceLastSave = true;
   protected transient int replicationType = WebMetaData.REPLICATION_TYPE_SYNC;
   protected transient boolean replicationTypeAlreadySet = false;

   /**
    * The logger for this session
    */
   private transient Logger log;

   public ClusteredSession(ClusterManager manager, Logger log)
   {
      this.manager = manager;
      this.log = log;
   }

   /**
    * Set the logger for this session
    * @param log the logger to use
    */
   protected void setLogger(Logger log)
   {
      this.log = log;
   }


   /**
    * Initialize fields marked as transient after loading this session
    * from the distributed store
    * @param manager the manager for this session
    * @param log the logger for this session
    */
   protected void initAfterLoad(ClusterManager manager, Logger log)
   {
      this.setManager(manager);
      this.setLogger(log);
      debug = 0;
      expiring = false;
      listeners = new ArrayList();
      notes = new HashMap();
      support = new PropertyChangeSupport(this);
      this.replicationType = manager.getReplicationType ();
      this.replicationTypeAlreadySet = false;


      // Notify all attributes of type HttpSessionActivationListener (SRV 7.7.2)
      this.activate();
   }
   // SerializableHttpSession-methods --------------------------------------------

   /**
    * Are attributes modified?
    * @param previousVersion The version to diff with
    */
   public boolean areAttributesModified(SerializableHttpSession previousVersion)
   {
     // return true;
      if (this == previousVersion)
      {
         if (log.isDebugEnabled())
            log.debug ("Are current session " + this.id + " attributes modified? : " + isSessionModifiedSinceLastSave);
         return isSessionModifiedSinceLastSave;
      }
      else
      {
         return true;
      }
   }

   /**
    * Get the creation time of this session
    */
   public long getContentCreationTime()
   {
      return this.getCreationTime();
   }

   /**
    * Get the last access time for this session
    */
   public long getContentLastAccessTime()
   {
      return this.getLastAccessedTime();
   }

   public void sessionHasBeenStored ()
   {
      isSessionModifiedSinceLastSave = false;
      replicationTypeAlreadySet = false;
   }

   public int getReplicationTypeForSession ()
   {
      return this.replicationType;
   }

   public void setReplicationTypeForSession (int type)
   {
      this.replicationType = type;
   }

   public boolean isReplicationTypeAlreadySet()
   {
      return replicationTypeAlreadySet;
   }
   // ----------------------------------------------------- Session Properties


   /**
    * Return the authentication type used to authenticate our cached
    * Principal, if any.
    */
   public String getAuthType()
   {

      return (this.authType);

   }


   /**
    * Set the authentication type used to authenticate our cached
    * Principal, if any.
    *
    * @param authType The new cached authentication type
    */
   public void setAuthType(String authType)
   {

      String oldAuthType = this.authType;
      this.authType = authType;
      support.firePropertyChange("authType", oldAuthType, this.authType);

   }


   /**
    * Set the creation time for this session.  This method is called by the
    * Manager when an existing Session instance is reused.
    *
    * @param time The new creation time
    */
   public void setCreationTime(long time)
   {

      this.creationTime = time;
      this.lastAccessedTime = time;
      this.thisAccessedTime = time;

      sessionIsDirty();
   }


   /**
    * Return the session identifier for this session.
    */
   public String getId()
   {

      return (this.id);

   }


   /**
    * Set the session identifier for this session.
    *
    * @param id The new session identifier
    */
   public void setId(String id)
   {

      if ((this.id != null) && (manager != null))
         manager.remove(this);

      this.id = id;

      if (manager != null)
         manager.add(this);

      // Notify interested session event listeners
      fireSessionEvent(Session.SESSION_CREATED_EVENT, null);

      // Notify interested application event listeners
      StandardContext context = (StandardContext) manager.getContainer();
      Object listeners[] = context.getApplicationListeners();
      if (listeners != null)
      {
         HttpSessionEvent event =
               new HttpSessionEvent(getSession());
         for (int i = 0; i < listeners.length; i++)
         {
            if (!(listeners[i] instanceof HttpSessionListener))
               continue;
            HttpSessionListener listener =
                  (HttpSessionListener) listeners[i];
            try
            {
               context.fireContainerEvent("beforeSessionCreated",
                     listener);
               listener.sessionCreated(event);
               context.fireContainerEvent("afterSessionCreated",
                     listener);
            }
            catch (Throwable t)
            {
               context.fireContainerEvent("afterSessionCreated",
                     listener);
               // FIXME - should we do anything besides log these?
               log(sm.getString("clusteredSession.sessionEvent"), t);
            }
         }
      }

   }


   /**
    * Return descriptive information about this Session implementation and
    * the corresponding version number, in the format
    * <code>&lt;description&gt;/&lt;version&gt;</code>.
    */
   public String getInfo()
   {

      return (this.info);

   }


   /**
    * Return the last time the client sent a request associated with this
    * session, as the number of milliseconds since midnight, January 1, 1970
    * GMT.  Actions that your application takes, such as getting or setting
    * a value associated with the session, do not affect the access time.
    */
   public long getLastAccessedTime()
   {

      return (this.lastAccessedTime);

   }


   /**
    * Return the Manager within which this Session is valid.
    */
   public Manager getManager()
   {

      return (this.manager);

   }


   /**
    * Set the Manager within which this Session is valid.
    *
    * @param manager The new Manager
    */
   public void setManager(Manager manager)
   {

      this.manager = manager;

   }


   /**
    * Return the maximum time interval, in seconds, between client requests
    * before the servlet container will invalidate the session.  A negative
    * time indicates that the session should never time out.
    *
    * @exception IllegalStateException if this method is called on
    *  an invalidated session
    */
   public int getMaxInactiveInterval()
   {

      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.getMaxInactiveInterval.ise"));

      return (this.maxInactiveInterval);

   }


   /**
    * Set the maximum time interval, in seconds, between client requests
    * before the servlet container will invalidate the session.  A negative
    * time indicates that the session should never time out.
    *
    * @param interval The new maximum interval
    */
   public void setMaxInactiveInterval(int interval)
   {

      this.maxInactiveInterval = interval;
   }


   /**
    * Set the <code>isNew</code> flag for this session.
    *
    * @param isNew The new value for the <code>isNew</code> flag
    */
   public void setNew(boolean isNew)
   {

      this.isNew = isNew;

   }


   /**
    * Return the authenticated Principal that is associated with this Session.
    * This provides an <code>Authenticator</code> with a means to cache a
    * previously authenticated Principal, and avoid potentially expensive
    * <code>Realm.authenticate()</code> calls on every request.  If there
    * is no current associated Principal, return <code>null</code>.
    */
   public Principal getPrincipal()
   {

      return (this.principal);

   }


   /**
    * Set the authenticated Principal that is associated with this Session.
    * This provides an <code>Authenticator</code> with a means to cache a
    * previously authenticated Principal, and avoid potentially expensive
    * <code>Realm.authenticate()</code> calls on every request.
    *
    * @param principal The new Principal, or <code>null</code> if none
    */
   public void setPrincipal(Principal principal)
   {

      Principal oldPrincipal = this.principal;
      this.principal = principal;
      support.firePropertyChange("principal", oldPrincipal, this.principal);

      if ( (oldPrincipal != null && ! oldPrincipal.equals(principal) ) ||
           (oldPrincipal == null && principal != null) )
         sessionIsDirty();

   }


   /**
    * Return the <code>HttpSession</code> for which this object
    * is the facade.
    */
   public HttpSession getSession()
   {

      if (facade == null)
         facade = new StandardSessionFacade(this);
      return (facade);

   }


   /**
    * Return the <code>isValid</code> flag for this session.
    */
   public boolean isValid()
   {

      return (this.isValid);

   }


   /**
    * Set the <code>isValid</code> flag for this session.
    *
    * @param isValid The new value for the <code>isValid</code> flag
    */
   public void setValid(boolean isValid)
   {

      this.isValid = isValid;
   }


   // ------------------------------------------------- Session Public Methods


   /**
    * Update the accessed time information for this session.  This method
    * should be called by the context when a request comes in for a particular
    * session, even if the application does not reference it.
    */
   public void access()
   {

      this.isNew = false;
      this.lastAccessedTime = this.thisAccessedTime;
      this.thisAccessedTime = System.currentTimeMillis();

   }


   /**
    * Add a session event listener to this component.
    */
   public void addSessionListener(SessionListener listener)
   {

      synchronized (listeners)
      {
         listeners.add(listener);
      }

   }


   /**
    * Perform the internal processing required to invalidate this session,
    * without triggering an exception if the session has already expired.
    */
   public void expire()
   {

      // Mark this session as "being expired" if needed
      if (expiring)
         return;
      expiring = true;
      setValid(false);

      // Remove this session from our manager's active sessions
      if (manager != null)
         manager.remove(this);

      // Unbind any objects associated with this session
      String keys[] = keys();
      for (int i = 0; i < keys.length; i++)
         removeAttribute(keys[i]);

      // Notify interested session event listeners
      fireSessionEvent(Session.SESSION_DESTROYED_EVENT, null);

      // Notify interested application event listeners
      // FIXME - Assumes we call listeners in reverse order
      StandardContext context = (StandardContext) manager.getContainer();
      Object listeners[] = context.getApplicationListeners();
      if (listeners != null)
      {
         HttpSessionEvent event =
               new HttpSessionEvent(getSession());
         for (int i = 0; i < listeners.length; i++)
         {
            int j = (listeners.length - 1) - i;
            if (!(listeners[j] instanceof HttpSessionListener))
               continue;
            HttpSessionListener listener =
                  (HttpSessionListener) listeners[j];
            try
            {
               context.fireContainerEvent("beforeSessionDestroyed",
                     listener);
               listener.sessionDestroyed(event);
               context.fireContainerEvent("afterSessionDestroyed",
                     listener);
            }
            catch (Throwable t)
            {
               context.fireContainerEvent("afterSessionDestroyed",
                     listener);
               // FIXME - should we do anything besides log these?
               log(sm.getString("clusteredSession.sessionEvent"), t);
            }
         }
      }

      // We have completed expire of this session
      expiring = false;
   }


    public void logout() {

        if (!isValid)
            throw new IllegalStateException
                (sm.getString("standardSession.isNew.ise"));

        // If the SingleSignOn didnt expire it, lets do it now.
        if (isValid) 
            expire();

    }


   /**
    * Perform the internal processing required to passivate
    * this session.
    */
   public void passivate()
   {

      // Notify ActivationListeners
      HttpSessionEvent event = null;
      String keys[] = keys();
      for (int i = 0; i < keys.length; i++)
      {
         Object attribute = getAttribute(keys[i]);
         if (attribute instanceof HttpSessionActivationListener)
         {
            if (event == null)
               event = new HttpSessionEvent(this);
            // FIXME: Should we catch throwables?
            ((HttpSessionActivationListener) attribute).sessionWillPassivate(event);
         }
      }

   }


   /**
    * Perform internal processing required to activate this
    * session.
    */
   public void activate()
   {

      // Notify ActivationListeners
      HttpSessionEvent event = null;
      String keys[] = keys();
      for (int i = 0; i < keys.length; i++)
      {
         Object attribute = getAttribute(keys[i]);
         if (attribute instanceof HttpSessionActivationListener)
         {
            if (event == null)
               event = new HttpSessionEvent(this);
            // FIXME: Should we catch throwables?
            ((HttpSessionActivationListener) attribute).sessionDidActivate(event);
         }
      }

   }


   /**
    * Return the object bound with the specified name to the internal notes
    * for this session, or <code>null</code> if no such binding exists.
    *
    * @param name Name of the note to be returned
    */
   public Object getNote(String name)
   {

      synchronized (notes)
      {
         return (notes.get(name));
      }

   }


   /**
    * Return an Iterator containing the String names of all notes bindings
    * that exist for this session.
    */
   public Iterator getNoteNames()
   {

      synchronized (notes)
      {
         return (notes.keySet().iterator());
      }

   }


   public void recycle()
   {
      // We do not recycle
   }


   /**
    * Remove any object bound to the specified name in the internal notes
    * for this session.
    *
    * @param name Name of the note to be removed
    */
   public void removeNote(String name)
   {

      synchronized (notes)
      {
         notes.remove(name);
      }

   }


   /**
    * Remove a session event listener from this component.
    */
   public void removeSessionListener(SessionListener listener)
   {

      synchronized (listeners)
      {
         listeners.remove(listener);
      }

   }


   /**
    * Bind an object to a specified name in the internal notes associated
    * with this session, replacing any existing binding for this name.
    *
    * @param name Name to which the object should be bound
    * @param value Object to be bound to the specified name
    */
   public void setNote(String name, Object value)
   {

      synchronized (notes)
      {
         notes.put(name, value);
      }

   }


   /**
    * Return a string representation of this object.
    */
   public String toString()
   {

      StringBuffer sb = new StringBuffer();
      sb.append("ClusteredSession[");
      sb.append(id);
      sb.append("]");
      return (sb.toString());

   }


   // ------------------------------------------------ Session Package Methods



   // ------------------------------------------------- HttpSession Properties


   /**
    * Return the time when this session was created, in milliseconds since
    * midnight, January 1, 1970 GMT.
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    */
   public long getCreationTime()
   {

      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.getCreationTime.ise"));

      return (this.creationTime);

   }


   /**
    * Return the ServletContext to which this session belongs.
    */
   public ServletContext getServletContext()
   {

      if (manager == null)
         return (null);
      Context context = (Context) manager.getContainer();
      if (context == null)
         return (null);
      else
         return (context.getServletContext());

   }


   /**
    * Return the session context with which this session is associated.
    *
    * @deprecated As of Version 2.1, this method is deprecated and has no
    *  replacement.  It will be removed in a future version of the
    *  Java Servlet API.
    */
   public HttpSessionContext getSessionContext()
   {

      if (sessionContext == null)
         sessionContext = new ClusteredSessionContext();
      return (sessionContext);

   }


   // ----------------------------------------------HttpSession Public Methods


   /**
    * Return the object bound with the specified name in this session, or
    * <code>null</code> if no object is bound with that name.
    *
    * @param name Name of the attribute to be returned
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    */
   public Object getAttribute(String name)
   {

      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.getAttribute.ise"));

      synchronized (attributes)
      {
         Object result = attributes.get(name);

         if (result != null)
         {
            int invalidationPolicy = ((ClusterManager)this.manager).getInvalidateSessionPolicy();

            if (invalidationPolicy == WebMetaData.SESSION_INVALIDATE_SET_AND_GET)
            {
               sessionIsDirty();
            }
            else if (invalidationPolicy == WebMetaData.SESSION_INVALIDATE_SET_AND_NON_PRIMITIVE_GET)
            {
               if ( ! (result instanceof String ||
                  result instanceof Integer ||
                  result instanceof Long ||
                  result instanceof Byte ||
                  result instanceof Short ||
                  result instanceof Float ||
                  result instanceof Double ||
                  result instanceof Character ||
                  result instanceof Boolean)
                  )
               {
                  sessionIsDirty();
               }
            }
         }

         return result;
      }

   }


   /**
    * Return an <code>Enumeration</code> of <code>String</code> objects
    * containing the names of the objects bound to this session.
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    */
   public Enumeration getAttributeNames()
   {

      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.getAttributeNames.ise"));

      synchronized (attributes)
      {
         return (new Enumerator(attributes.keySet()));
      }

   }


   /**
    * Return the object bound with the specified name in this session, or
    * <code>null</code> if no object is bound with that name.
    *
    * @param name Name of the value to be returned
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    *
    * @deprecated As of Version 2.2, this method is replaced by
    *  <code>getAttribute()</code>
    */
   public Object getValue(String name)
   {

      return (getAttribute(name));

   }


   /**
    * Return the set of names of objects bound to this session.  If there
    * are no such objects, a zero-length array is returned.
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    *
    * @deprecated As of Version 2.2, this method is replaced by
    *  <code>getAttributeNames()</code>
    */
   public String[] getValueNames()
   {

      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.getValueNames.ise"));

      return (keys());

   }


   /**
    * Invalidates this session and unbinds any objects bound to it.
    *
    * @exception IllegalStateException if this method is called on
    *  an invalidated session
    */
   public void invalidate()
   {

      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.invalidate.ise"));

      // Cause this session to expire
      expire();

   }


   /**
    * Return <code>true</code> if the client does not yet know about the
    * session, or if the client chooses not to join the session.  For
    * example, if the server used only cookie-based sessions, and the client
    * has disabled the use of cookies, then a session would be new on each
    * request.
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    */
   public boolean isNew()
   {

      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.isNew.ise"));

      return (this.isNew);

   }


   /**
    * Bind an object to this session, using the specified name.  If an object
    * of the same name is already bound to this session, the object is
    * replaced.
    * <p>
    * After this method executes, and if the object implements
    * <code>HttpSessionBindingListener</code>, the container calls
    * <code>valueBound()</code> on the object.
    *
    * @param name Name to which the object is bound, cannot be null
    * @param value Object to be bound, cannot be null
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    *
    * @deprecated As of Version 2.2, this method is replaced by
    *  <code>setAttribute()</code>
    */
   public void putValue(String name, Object value)
   {

      setAttribute(name, value);

   }


   /**
    * Remove the object bound with the specified name from this session.  If
    * the session does not have an object bound with this name, this method
    * does nothing.
    * <p>
    * After this method executes, and if the object implements
    * <code>HttpSessionBindingListener</code>, the container calls
    * <code>valueUnbound()</code> on the object.
    *
    * @param name Name of the object to remove from this session.
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    */
   public void removeAttribute(String name)
   {

      // Validate our current state
      if (!expiring && !isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.removeAttribute.ise"));

      // Remove this attribute from our collection
      Object value = null;
      boolean found = false;
      synchronized (attributes)
      {
         found = attributes.containsKey(name);
         if (found)
         {
            value = attributes.get(name);
            attributes.remove(name);

            // Session is now dirty
            sessionIsDirty ();
         }
         else
         {
            return;
         }
      }

      // Call the valueUnbound() method if necessary
      HttpSessionBindingEvent event =
            new HttpSessionBindingEvent(this, name, value);
      if ((value != null) &&
            (value instanceof HttpSessionBindingListener))
         ((HttpSessionBindingListener) value).valueUnbound(event);

      // Notify interested application event listeners
      StandardContext context = (StandardContext) manager.getContainer();
      Object listeners[] = context.getApplicationListeners();
      if (listeners == null)
         return;
      for (int i = 0; i < listeners.length; i++)
      {
         if (!(listeners[i] instanceof HttpSessionAttributeListener))
            continue;
         HttpSessionAttributeListener listener =
               (HttpSessionAttributeListener) listeners[i];
         try
         {
            context.fireContainerEvent("beforeSessionAttributeRemoved",
                  listener);
            listener.attributeRemoved(event);
            context.fireContainerEvent("afterSessionAttributeRemoved",
                  listener);
         }
         catch (Throwable t)
         {
            context.fireContainerEvent("afterSessionAttributeRemoved",
                  listener);
            // FIXME - should we do anything besides log these?
            log(sm.getString("clusteredSession.attributeEvent"), t);
         }
      }
   }


   /**
    * Remove the object bound with the specified name from this session.  If
    * the session does not have an object bound with this name, this method
    * does nothing.
    * <p>
    * After this method executes, and if the object implements
    * <code>HttpSessionBindingListener</code>, the container calls
    * <code>valueUnbound()</code> on the object.
    *
    * @param name Name of the object to remove from this session.
    *
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    *
    * @deprecated As of Version 2.2, this method is replaced by
    *  <code>removeAttribute()</code>
    */
   public void removeValue(String name)
   {

      removeAttribute(name);

   }


   /**
    * Bind an object to this session, using the specified name.  If an object
    * of the same name is already bound to this session, the object is
    * replaced.
    * <p>
    * After this method executes, and if the object implements
    * <code>HttpSessionBindingListener</code>, the container calls
    * <code>valueBound()</code> on the object.
    *
    * @param name Name to which the object is bound, cannot be null
    * @param value Object to be bound, cannot be null
    *
    * @exception IllegalArgumentException if an attempt is made to add a
    *  non-serializable object in an environment marked distributable.
    * @exception IllegalStateException if this method is called on an
    *  invalidated session
    */
   public void setAttribute(String name, Object value)
   {

      // Name cannot be null
      if (name == null)
         throw new IllegalArgumentException
               (sm.getString("clusteredSession.setAttribute.namenull"));

      // Null value is the same as removeAttribute()
      if (value == null)
      {
         removeAttribute(name);
         return;
      }

      // Validate our current state
      if (!isValid)
         throw new IllegalStateException
               (sm.getString("clusteredSession.setAttribute.ise"));
      if ((manager != null) && manager.getDistributable() &&
            !(value instanceof Serializable))
         throw new IllegalArgumentException
               (sm.getString("clusteredSession.setAttribute.iae"));

      // Replace or add this attribute
      Object unbound = null;
      synchronized (attributes)
      {
         unbound = attributes.get(name);
         attributes.put(name, value);
      }

      // Call the valueUnbound() method if necessary
      if ((unbound != null) &&
            (unbound instanceof HttpSessionBindingListener))
      {
         ((HttpSessionBindingListener) unbound).valueUnbound
               (new HttpSessionBindingEvent(this, name));
      }

      // Call the valueBound() method if necessary
      HttpSessionBindingEvent event = null;
      if (unbound != null)
         event = new HttpSessionBindingEvent
               (this, name, unbound);
      else
         event = new HttpSessionBindingEvent
               (this, name, value);
      if (value instanceof HttpSessionBindingListener)
         ((HttpSessionBindingListener) value).valueBound(event);

      // Notify interested application event listeners
      StandardContext context = (StandardContext) manager.getContainer();
      Object listeners[] = context.getApplicationListeners();
      if (listeners == null)
         return;
      for (int i = 0; i < listeners.length; i++)
      {
         if (!(listeners[i] instanceof HttpSessionAttributeListener))
            continue;
         HttpSessionAttributeListener listener =
               (HttpSessionAttributeListener) listeners[i];
         try
         {
            if (unbound != null)
            {
               context.fireContainerEvent("beforeSessionAttributeReplaced",
                     listener);
               listener.attributeReplaced(event);
               context.fireContainerEvent("afterSessionAttributeReplaced",
                     listener);
            }
            else
            {
               context.fireContainerEvent("beforeSessionAttributeAdded",
                     listener);
               listener.attributeAdded(event);
               context.fireContainerEvent("afterSessionAttributeAdded",
                     listener);
            }
         }
         catch (Throwable t)
         {
            if (unbound != null)
               context.fireContainerEvent("afterSessionAttributeReplaced",
                     listener);
            else
               context.fireContainerEvent("afterSessionAttributeAdded",
                     listener);
            // FIXME - should we do anything besides log these?
            log(sm.getString("clusteredSession.attributeEvent"), t);
         }
      }

      // Session is dirty
      sessionIsDirty ();
   }


   // -------------------------------------------- HttpSession Private Methods


   // -------------------------------------------------------- Private Methods


   /**
    * Notify all session event listeners that a particular event has
    * occurred for this Session.  The default implementation performs
    * this notification synchronously using the calling thread.
    *
    * @param type Event type
    * @param data Event data
    */
   public void fireSessionEvent(String type, Object data)
   {

      if (listeners.size() < 1)
         return;
      SessionEvent event = new SessionEvent(this, type, data);
      SessionListener list[] = new SessionListener[0];
      synchronized (listeners)
      {
         list = (SessionListener[]) listeners.toArray(list);
      }
      for (int i = 0; i < list.length; i++)
         (list[i]).sessionEvent(event);

   }


   /**
    * Return the names of all currently defined session attributes
    * as an array of Strings.  If there are no defined attributes, a
    * zero-length array is returned.
    */
   private String[] keys()
   {

      String results[] = new String[0];
      synchronized (attributes)
      {
         return ((String[]) attributes.keySet().toArray(results));
      }

   }


   /**
    * Log a message on the Logger associated with our Manager (if any).
    *
    * @param message Message to be logged
    */
   private void log(String message)
   {
      log.debug(message);
   }


   /**
    * Log a message on the Logger associated with our Manager (if any).
    *
    * @param message Message to be logged
    * @param throwable Associated exception
    */
   private void log(String message, Throwable throwable)
   {
      log.error(message, throwable);
   }


    private void writeObject(java.io.ObjectOutputStream out)
        throws IOException
    {
       out.defaultWriteObject();
    }
    private void readObject(java.io.ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
       in.defaultReadObject();
    }

   protected void sessionIsDirty ()
   {
      // Session is dirty
      isSessionModifiedSinceLastSave = true;
   }
}


// -------------------------------------------------------------- Private Class


/**
 * This class is a dummy implementation of the <code>HttpSessionContext</code>
 * interface, to conform to the requirement that such an object be returned
 * when <code>HttpSession.getSessionContext()</code> is called.
 *
 * @author Craig R. McClanahan
 *
 * @deprecated As of Java Servlet API 2.1 with no replacement.  The
 *  interface will be removed in a future version of this API.
 */

final class ClusteredSessionContext implements HttpSessionContext
{


   private HashMap dummy = new HashMap();

   /**
    * Return the session identifiers of all sessions defined
    * within this context.
    *
    * @deprecated As of Java Servlet API 2.1 with no replacement.
    *  This method must return an empty <code>Enumeration</code>
    *  and will be removed in a future version of the API.
    */
   public Enumeration getIds()
   {

      return (new Enumerator(dummy));

   }


   /**
    * Return the <code>HttpSession</code> associated with the
    * specified session identifier.
    *
    * @param id Session identifier for which to look up a session
    *
    * @deprecated As of Java Servlet API 2.1 with no replacement.
    *  This method must return null and will be removed in a
    *  future version of the API.
    */
   public HttpSession getSession(String id)
   {

      return (null);

   }


}
