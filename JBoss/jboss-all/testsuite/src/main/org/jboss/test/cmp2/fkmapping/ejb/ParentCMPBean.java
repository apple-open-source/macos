/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkmapping.ejb;


import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.ejb.CreateException;


/**
 * @ejb.bean
 *    name="Parent"
 *    type="CMP"
 *    cmp-version="2.x"
 *    view-type="local"
 *    reentrant="false"
 * @ejb.pk generate="true"
 * @ejb.util  generate="physical"
 * @ejb.persistence  table-name="PARENT"
 * @jboss.persistence
 *    create-table="true"
 *    remove-table="true"
 * @ejb:transaction-type  type="Container"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class ParentCMPBean
   implements EntityBean
{
   // Attributes -----------------------------------------------
   private EntityContext ctx;

   // CMP accessors --------------------------------------------
   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence  column-name="PARENT_ID"
    */
   public abstract Long getId();
   public abstract void setId(Long id);

   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="FIRST_NAME"
    */
   public abstract String getFirstName();
   public abstract void setFirstName(String name);

   /**
    * One-to-one unidirectional
    *
    * @ejb.interface-method
    * @ejb.relation
    *    name="Parent-Child"
    *    role-name="Parent-has-Child"
    *    target-ejb="Child"
    *    target-role-name="Child-belongsto-Parent"
    * @jboss.relation
    *    related-pk-field="id"
    *    fk-column="CHILD_FK"
    */
   public abstract ChildLocal getChild();
   /**
    * @ejb.interface-method
    */
   public abstract void setChild(ChildLocal child);

   // EntityBean implementation -------------------------------------
   /**
    * @ejb.create-method
    * @throws CreateException
    */
   public ParentPK ejbCreate(Long parentId, String firstName)
      throws CreateException
   {
      setId(parentId);
      setFirstName(firstName);
      return null;
   }

   public void ejbPostCreate(Long parentId, String firstName)
   {
   }

   /**
    * @param  ctx The new entityContext value
    */
   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }

   /**
    * Unset the associated entity context.
    */
   public void unsetEntityContext()
   {
      this.ctx = null;
   }

   public void ejbActivate() {}
   public void ejbLoad() {}
   public void ejbPassivate() {}
   public void ejbRemove() throws RemoveException {}
   public void ejbStore() {}
}
