/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.beanimpl.ejb;

import java.io.Serializable;

/**
 * CMP 1.1 concrete implementation for the HTTPSession bean.
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSession
 * @see org.jboss.ha.httpsession.beanimpl.ejb.ClusteredHTTPSessionBeanImpl
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>31. decembre 2001 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public class ClusteredHTTPSessionBeanCmp11 extends ClusteredHTTPSessionBeanImpl
{

   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   public String id;
   public Serializable serializedSession;
   public long lastAccessTime;
   public long creationTime;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
       
   public ClusteredHTTPSessionBeanCmp11 ()
    {       
    }

   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // ClusteredHTTPSessionBeanCmp11 overrides ---------------------------------------------------
   
   public String getSessionId ()
   { return this.id; }
   
   public void setSessionId (String sessionId)
   { this.id = sessionId; }
   
   public Serializable getSerializedSession ()
   { return this.serializedSession; }
   
   public void setSerializedSession (Serializable session)
   { this.serializedSession = session; }
   
   public long getLastAccessedTime () 
   { return this.lastAccessTime; }
   
   public void setLastAccessedTime (long value)
   { this.lastAccessTime = value; }
   
   public long getCreationTime () 
   { return this.creationTime; }
   
   public void setCreationTime (long value)
   { this.creationTime = value; }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

}
