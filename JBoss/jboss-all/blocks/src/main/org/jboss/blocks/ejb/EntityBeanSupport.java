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

import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.EJBContext;
import javax.ejb.EJBException;
import javax.ejb.EJBObject;
import javax.ejb.RemoveException;

import org.jboss.logging.Logger;

/**
 * A base support class for an <em>EJB entity bean</em>.
 *
 * <p>
 * Fulfills all methods required for the {@link EntityBean} interface 
 * including basic {@link EntityContext} handling and provides
 * helper methods for common entity actions.
 *
 * <p>
 * Also provides support for <tt>isModified</tt> if the bean developer
 * chooses to make use of it.
 *
 * @see javax.ejb.EntityBean
 * 
 * @version <tt>$Revision: 1.1.1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class EntityBeanSupport
   implements EntityBean
{
   /** Instance logger. */
   protected Logger log = Logger.getLogger(getClass());
   
   /** Entity context */
   private EntityContext entityContext;

   /** Persisent fields modified flag */
   private boolean modified;

   /**
    * Set the entity context.
    *
    * @param context    Entity context.
    */
   public void setEntityContext(final EntityContext context)
      throws EJBException, RemoteException
   {
      entityContext = context;
   }

   /**
    * Get the entity context.
    *
    * @return     Entity context.
    *
    * @throws IllegalStateException    Entity context is invalid.
    */
   public EntityContext getEntityContext()
      throws EJBException
   {
      if (entityContext == null)
         throw new IllegalStateException("entity context is invalid");

      return entityContext;
   }

   /**
    * Get the EJB context.
    *
    * @return     EJB context.
    */
   protected EJBContext getEJBContext()
      throws EJBException
   {
      return getEntityContext();
   }

   /**
    * Unset the entity context.
    */
   public void unsetEntityContext()
      throws EJBException, RemoteException
   {
      entityContext = null;
   }

   /**
    * Change the modified flag.
    */
   protected void setModified(final boolean flag) {
      modified = flag;
   }

   /**
    * Mark this bean as modified.
    */
   protected void setModified() {
      setModified(true);
   }
   
   /**
    * Check if this bean has been modified.
    *
    * @return  The value of the modified flag.
    */
   public boolean isModified() {
      return modified;
   }

   /**
    * Helper method to get the EJBObject associated with this Entity bean.
    *
    * <p>Same as <code>getEntityContext().getEJBObject()</code>.
    *
    * @return  An EJBObject assocaiated with this bean.
    */
   public EJBObject getEJBObject() throws IllegalStateException {
      return getEntityContext().getEJBObject();
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
   public void ejbRemove() throws EJBException, RemoveException, RemoteException {}

   /**
    * Non-operation.
    */
   public void ejbLoad() throws EJBException, RemoteException {}

   /**
    * Clears the modified flag.
    */
   public void ejbStore() throws EJBException, RemoteException {
      // reset the modified flag
      setModified(false);
   }
}
