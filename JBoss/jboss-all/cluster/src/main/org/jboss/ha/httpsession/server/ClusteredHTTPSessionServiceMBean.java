/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.server;

import javax.ejb.EJBException;

import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;

/**
 * Service that provide unified access to clustered HTTPSessions for servlets.
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSession
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionHome
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.4.2 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>31. décembre 2001 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public interface ClusteredHTTPSessionServiceMBean extends org.jboss.system.ServiceMBean
{
   /**
    * Return the HttpSession associated to a session id.
    * As all session id are shared for all Web Applications, all sessions ids accross
    * all applications and all nodes must be distincts!
    * The creation and last access time of the session may not be correct if they
    * are the only thing that has been modified on a distant node (and no attribute).
    * See setHttpSession for more information.
    */   
   public SerializableHttpSession getHttpSession (String sessionId, ClassLoader tcl) throws EJBException;
   /**
    * Associate a new session with the session id. To reduce the cluster communication,
    * if the only thing that has changed in the session is the last accessed time, the
    * new session is kept in cache but not replicated on the other nodes. Thus, if you
    * use a front-end load-balancer that support sticky session, that is not a problem
    * because a client will always target the same node and receive the updated session
    * available in cache.
    * Nevertheless, as soon as an attribute is modified in the session, it is replicated
    * in the cluster.
    */   
   public void setHttpSession (String sessionId, SerializableHttpSession session) throws EJBException;
   /**
    * Remove an HttpSession from the cluster (log off for example)
    */   
   public void removeHttpSession (String sessionId) throws EJBException;   
   /**
    * Generates a new session id available cluster-wide
    */   
   public String getSessionId ();
   
   /**
    * Indicate the duration, in ms, after which the session can be cleaned if no
    * access occurs.
    */   
   public long getSessionTimeout (); // defaults to 15 minutes i.e. 15*60*1000 = 900'000
   /**
    * Indicate the duration, in ms, after which the session can be cleaned if no
    * access occurs.
    */   
   public void setSessionTimeout (long miliseconds);   
   
   /**
    * Indicates whether the service should use the local and home interfaces of the
    * entity bean or the remote and remote home interfaces (depending if they are in
    * the same JVM).
    */   
   public void setUseLocalBean (boolean useLocal);   
   public boolean getUseLocalBean ();
}
