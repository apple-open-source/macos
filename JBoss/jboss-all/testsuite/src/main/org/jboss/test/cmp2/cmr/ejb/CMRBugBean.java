/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.cmp2.cmr.ejb;


import java.util.Collection;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

import org.jboss.test.cmp2.cmr.interfaces.CMRBugEJBLocal;

/**
 *  class <code>CMRBugBean</code> demonstrates bug 523627.  CMR fields may get changed
 * in ejbPostCreate, so newly created entities must be marked as needing ejbSave.
 * Currently this is done by putting them in GlobalTxEntityMap.
 *
 * @author <a href="mailto:MNewcomb@tacintel.com">Michael Newcomb</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version 1.0
 *
 * @ejb:bean   name="CMRBugEJB"
 *             local-jndi-name="LocalReadOnly"
 *             view-type="local"
 *             type="CMP"
 *             cmp-version="2.x"
 *             primkey-field="id"
 *             reentrant="true"
 *             schema="CMRBug"
 * @ejb:pk class="java.lang.String"
 * @ejb:finder signature="java.util.Collection findAll()"
 *             query="select object(cmr_bug) from CMRBug as cmr_bug"
 * @jboss:create-table create="true"
 * @jboss:remove-table remove="true"
 */
public abstract class CMRBugBean
   implements EntityBean
{
   private EntityContext context;

   /**
    * Describe <code>getId</code> method here.
    *
    * @return a <code>String</code> value
    * @ejb:persistent-field
    * @ejb:interface-method
    */
   public abstract String getId();

   /**
    * Describe <code>setId</code> method here.
    *
    * @param id a <code>String</code> value
    * @ejb:interface-method
    */
   public abstract void setId(String id);

   /**
    * Describe <code>getDescription</code> method here.
    *
    * @return a <code>String</code> value
    * @ejb:persistent-field
    * @ejb:interface-method
    */
   public abstract String getDescription();

   /**
    * Describe <code>setDescription</code> method here.
    *
    * @param description a <code>String</code> value
    * @ejb:interface-method
    */
   public abstract void setDescription(String description);

   /**
    * Describe <code>getParent</code> method here.
    *
    * @return a <code>CMRBugEJBLocal</code> value
    * @ejb:interface-method
    * @ejb:relation name="CMRBug-CMRBug"
    *               role-name="parent"
    *               cascade-delete="yes"
    * @jboss:auto-key-fields
    */
   public abstract CMRBugEJBLocal getParent();

   /**
    * Describe <code>setParent</code> method here.
    *
    * @param parent a <code>CMRBugEJBLocal</code> value
    * @ejb:interface-method
    */
   public abstract void setParent(CMRBugEJBLocal parent);

   /**
    * Describe <code>getChildren</code> method here.
    *
    * @return a <code>Collection</code> value
    * @ejb:interface-method
    * @ejb:relation name="CMRBug-CMRBug"
    *               role-name="children"
    *               multiple="yes"
    * @jboss:auto-key-fields
    */
   public abstract Collection getChildren();

   /**
    * Describe <code>setChildren</code> method here.
    *
    * @param children a <code>Collection</code> value
    * @ejb:interface-method
    */
   public abstract void setChildren(Collection children);

   /**
    * Describe <code>addChild</code> method here.
    *
    * @param child a <code>CMRBugEJBLocal</code> value
    * @return a <code>boolean</code> value
    * @ejb:interface-method
    */
   public boolean addChild(CMRBugEJBLocal child)
   {
      try
      {
         Collection children = getChildren();
         return children.add(child);
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
   }

   /**
    * Describe <code>removeChild</code> method here.
    *
    * @param child a <code>CMRBugEJBLocal</code> value
    * @return a <code>boolean</code> value
    * @ejb:interface-method
    */
   public boolean removeChild(CMRBugEJBLocal child)
   {
      try
      {
         Collection children = getChildren();
         return children.remove(child);
      }
      catch(Exception e)
      {
         throw new EJBException(e);
      }
   }

   //
   // The following is the linked list implementation. It is implemented with unidirectional CMRs.
   // These are used to test correct foreign key state initialization when the foreign key
   // loaded is not a valid value, i.e. the relationship was already changed in the tx.
   //

   /**
    * @ejb:interface-method view-type="local"
    * @ejb:relation
    *    name="viewcomponent-prevnode"
    *    role-name="one-viewcomponent-has-one-previous-node"
    *    target-role-name="one-prev-belogs-to-one-viewcomponent"
    *    target-ejb="CMRBugEJB"
    *    target-multiple="no"
    * @jboss:relation
    *   related-pk-field="id"
    *   fk-column="prev_id_fk"
    */
   public abstract CMRBugEJBLocal getPrevNode();
   /**
    * @ejb:interface-method view-type="local"
    */
   public abstract void setPrevNode(CMRBugEJBLocal a_ViewComponent);

   /**
    * @ejb:interface-method view-type="local"
    * @ejb:relation
    *    name="viewcomponent-nextnode"
    *    role-name="one-viewcomponent-has-one-following-node"
    *    target-role-name="one-following-node-belogs-to-one-viewcomponent"
    *    target-ejb="CMRBugEJB"
    *    target-multiple="no"
    * @jboss:relation
    *   related-pk-field="id"
    *   fk-column="next_id_fk"
    */
   public abstract CMRBugEJBLocal getNextNode();
   /**
    * @ejb:interface-method view-type="local"
    */
   public abstract void setNextNode(CMRBugEJBLocal a_ViewComponent);

   // --------------------------------------------------------------------------
   // EntityBean methods
   //

   /**
    * Describe <code>ejbCreate</code> method here.
    *
    * @param id a <code>String</code> value
    * @param description a <code>String</code> value
    * @param parent a <code>CMRBugEJBLocal</code> value
    * @return an <code>Integer</code> value
    * @exception CreateException if an error occurs
    * @ejb:create-method
    */
   public String ejbCreate(String id, String description, CMRBugEJBLocal parent)
      throws CreateException
   {
      setId(id);
      setDescription(description);

      // CMP beans return null for this method
      //
      return null;
   }

   /**
    * Describe <code>ejbPostCreate</code> method here.
    *
    * @param id a <code>String</code> value
    * @param description a <code>String</code> value
    * @param parent a <code>CMRBugEJBLocal</code> value
    * @exception CreateException if an error occurs
    */
   public void ejbPostCreate(String id, String description, CMRBugEJBLocal parent)
      throws CreateException
   {
      // must set the CMR fields in the post create
      //
      setParent(parent);
   }

   public void setEntityContext(EntityContext context)
   {
      this.context = context;
   }

   public void unsetEntityContext()
   {
      context = null;
   }

   public void ejbRemove()
   {
   }

   public void ejbLoad()
   {
   }

   public void ejbStore()
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }
}
