/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc5.session;

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
import org.apache.catalina.session.StandardSession;
import org.apache.catalina.session.StandardSessionFacade;
import org.jboss.metadata.WebMetaData;
import org.jboss.logging.Logger;

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
 @version $Revision: 1.1.2.1 $
 */
class ClusteredSession
    extends StandardSession
    implements SerializableHttpSession
{
    private static final long serialVersionUID = -758573655613558722L;
    private static Logger log = Logger.getLogger(ClusteredSession.class);

   // ----------------------------------------------------- Instance Variables


   /**
    * Descriptive information describing this Session implementation.
    */
   protected static final String info = "ClusteredSession/1.0";


   /**
    * The Manager with which this Session is associated.
    */
   private transient Manager manager = null;


   /**
    * The string manager for this package.
    */
   private static StringManager sm =
         StringManager.getManager("org.jboss.web.tomcat.session");

   protected transient boolean isSessionModifiedSinceLastSave = true;
   protected transient int replicationType = WebMetaData.REPLICATION_TYPE_SYNC;
   protected transient boolean replicationTypeAlreadySet = false;

   public ClusteredSession(JBossManager manager)
   {
       super(manager);
      this.manager = manager;
   }

   /**
    * Initialize fields marked as transient after loading this session
    * from the distributed store
    * @param manager the manager for this session
    * @param log the logger for this session
    */
   protected void initAfterLoad(JBossManager manager)
   {
      setManager(manager);
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
    * Set the creation time for this session.  This method is called by the
    * Manager when an existing Session instance is reused.
    *
    * @param time The new creation time
    */
   public void setCreationTime(long time)
   {
       super.setCreationTime(time);
       sessionIsDirty();
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


   // ------------------------------------------------- Session Public Methods


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
            int invalidationPolicy = ((JBossManager)this.manager).getInvalidateSessionPolicy();

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
   public void removeAttribute(String name, boolean notify)
   {

      // Remove this attribute from our collection
      Object value = null;
      boolean found = false;
      synchronized (attributes)
      {
         found = attributes.containsKey(name);
         if (found)
         {
            // Session is now dirty
            sessionIsDirty ();
         }
         else
         {
            return;
         }
      }

      super.removeAttribute(name, notify);

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

       super.setAttribute(name, value);

       // Session is dirty
       sessionIsDirty ();
   }


   /**
    * Log a message on the Logger associated with our Manager (if any).
    *
    * @param message Message to be logged
    */
   protected void log(String message)
   {
      log.debug(message);
   }


   /**
    * Log a message on the Logger associated with our Manager (if any).
    *
    * @param message Message to be logged
    * @param throwable Associated exception
    */
   protected void log(String message, Throwable throwable)
   {
      log.error(message, throwable);
   }


    protected void writeObject(java.io.ObjectOutputStream out)
        throws IOException
    {
       out.defaultWriteObject();
    }
    protected void readObject(java.io.ObjectInputStream in)
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
