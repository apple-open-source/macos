/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.beanimpl.ejb;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.RemoveException;

import java.rmi.RemoteException;
import java.io.Serializable;

import org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionBusiness;
import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;

/**
 * Abstract default implementation of the Clustered HTTP session for servlets.
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSession
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionBusiness
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionHome
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>31. decembre 2001 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public abstract class ClusteredHTTPSessionBeanAbstract implements javax.ejb.EntityBean, ClusteredHTTPSessionBusiness
{

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   protected javax.ejb.EntityContext ejbContext;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   public String ejbCreate (String sessionId) throws CreateException
   {
      this.setSessionId (sessionId);
      return null;
   }
   public void ejbPostCreate (String sessionId) throws CreateException
   {}

   public String ejbCreate (String sessionId, SerializableHttpSession session) throws CreateException
   {
      this.setSessionId (sessionId);
      this.setSession (session);
      return null;
   }
   public void ejbPostCreate (String sessionId, SerializableHttpSession session) throws CreateException
   {}

   // Optimisation: called by the CMP engine
   //
   public abstract boolean isModified ();

   // EntityBean implementation ----------------------------------------------

   public void ejbStore () throws EJBException, RemoteException
   {}
   public void ejbActivate () throws EJBException, RemoteException
   {}
   public void ejbPassivate () throws EJBException, RemoteException
   {}
   public void ejbLoad () throws EJBException, RemoteException
   {}
   public void setEntityContext (javax.ejb.EntityContext ctx)
   {
      ejbContext = ctx;
   }
   public void ejbRemove () throws RemoveException, EJBException, RemoteException
   {}
   public void unsetEntityContext ()
   {
      ejbContext = null;
   }

   // ClusteredHTTPSessionBusiness implementation ----------------------------------------------

   public abstract String getSessionId ();
   public abstract void setSessionId (String sessionId);

   public abstract Serializable getSerializedSession ();
   public abstract void setSerializedSession (Serializable session);

   public abstract long getLastAccessedTime ();
   public abstract void setLastAccessedTime (long value);
   public abstract long getCreationTime ();
   public abstract void setCreationTime (long value);

   // This field is not directly stored by the CMP engine
   //
   public abstract SerializableHttpSession getSession () throws EJBException;
   public abstract void setSession (SerializableHttpSession session);

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   protected javax.ejb.EntityContext getEntityContext ()
   {
      return ejbContext;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
