/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.blocks.ejb;

import java.rmi.RemoteException;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.EJBContext;
import javax.ejb.EJBException;
import javax.ejb.EJBObject;

import org.jboss.logging.Logger;

/**
 * A base support class for an <em>EJB session bean</em>.
 *
 * <p>
 * Fulfills all methods required for the {@link SessionBean} interface
 * including basic {@link SessionContext} handling.
 *
 * @see javax.ejb.SessionBean
 *
 * @version <tt>$Revision: 1.1.1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class SessionBeanSupport
   implements SessionBean
{
   /** Instance logger. */
   protected Logger log = Logger.getLogger(getClass());

   /** Session context */
   private SessionContext sessionContext;

   /**
    * Set the session context.
    *
    * @param context    Session context
    */
   public void setSessionContext(final SessionContext context)
      throws EJBException, RemoteException
   {
      sessionContext = context;
   }

   /**
    * Get the session context.
    *
    * @return     Session context
    *
    * @throws IllegalStateException    Session context is invalid
    */
   public SessionContext getSessionContext()
      throws EJBException
   {
      if (sessionContext == null)
         throw new IllegalStateException("session context is invalid");

      return sessionContext;
   }

   /**
    * Get the EJB context.
    *
    * @return     EJB context.
    */
   protected EJBContext getEJBContext()
      throws EJBException
   {
      return getSessionContext();
   }

   /**
    * Helper method to get the EJBObject associated with this Session bean.
    *
    * <p>Same as <code>getSessionContext().getEJBObject()</code>.
    *
    * @return  EJBObject
    */
   public EJBObject getEJBObject() {
      return getSessionContext().getEJBObject();
   }

   /**
    * Non-operation.
    */
   public void ejbActivate() throws EJBException, RemoteException {}

   /**
    * Non-operation.
    */
   public void ejbPassivate() throws EJBException, RemoteException {}
   
   /**
    * Non-operation.
    */
   public void ejbRemove() throws EJBException, RemoteException {}
}
