/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.beanimpl.interfaces;

import java.rmi.RemoteException;

import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;

/**
 * Business methods for the entity bean that will store HTTPSession
 * in a clustered environment.
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSession
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>31. décembre 2001 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public interface ClusteredHTTPSessionBusiness
{

   /**
    * Get the session identifier associated to this HTTPSession.
    * This is the primary key of the entity bean.
    * @return The session id.
    */   
  	public String getSessionId () throws RemoteException; // PK

   /**
    * Return the HttpSession object associated to its id. The main difference with the
    * standard class is that this one is Serializable.
    */   
   public SerializableHttpSession getSession () throws RemoteException;
   /**
    * Associate a new session (set of attributes, ...) to this id.
    */   
   public void setSession (SerializableHttpSession session) throws RemoteException;
   
   // used to clean timeouted sessions without accessing the HttpSession object
   //
   /**
    * Return the last time this session has been accessed in miliseconds since 1970.
    * This method is a shortcut for getSession().getLastAccessedTime (). The reason
    * is that the bean, when directly asked for the time, don't need to deserialize
    * the session representation if not already done (lazy deserialization).
    * If the only thing that changes in an HTTPSession it the last accessed time (and no attributes),
    * the session may not be replicated on other node (to reduce traffic). Nevertheless,
    * the new session is stored in the local bean. Consequently, if a load-balancer
    * with sticky sessions is used, this is no problem (the local, updated, bean is used.
    */   
   public long getLastAccessedTime() throws RemoteException;
   /**
    * Return the time when this session has been created in miliseconds since 1970.
    * This method is a shortcut for getSession().getLastAccessedTime (). The reason
    * is that the bean, when directly asked for the time, don't need to deserialize
    * the session representation if not already done (lazy deserialization)
    */   
   public long getCreationTime() throws RemoteException;  

}
