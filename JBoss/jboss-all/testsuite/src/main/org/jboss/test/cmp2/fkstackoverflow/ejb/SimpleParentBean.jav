/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkstackoverflow.ejb;

import org.apache.log4j.Category;

import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.ejb.CreateException;
import java.util.Collection;


/**
 * @ejb.bean
 *    name="SimpleParent"
 *    type="CMP"
 *    cmp-version="2.x"
 *    view-type="local"
 *    reentrant="false"
 *    primkey-field="id"
 * @ejb.pk generate="false"
 * @ejb.util  generate="physical"
 * @ejb.persistence  table-name="PARENT"
 * @ejb:transaction  type="Required"
 * @ejb:transaction-type  type="Container"
 * @jboss.persistence
 *    create-table="true"
 *    remove-table="true"
 */
public abstract class SimpleParentBean
   implements EntityBean
{
   Category log = Category.getInstance(SimpleParentBean.class);
   private EntityContext ctx;

   // CMP accessors

   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence  column-name="PARENT_ID"
    */
   public abstract Long getId();
   public abstract void setId(Long id);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="FIRST_NAME"
    */
   public abstract String getFirstName();
   /**
    * @ejb.interface-method
    */
   public abstract void setFirstName(String firstName);

   // CMR

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="parent-children-simple1"
    *    role-name="parent-has-children"
    */
   public abstract Collection getChildren1();
   /**
    * @ejb.interface-method
    */
   public abstract void setChildren1(Collection children);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="parent-children-simple2"
    *    role-name="parent-has-children"
    */
   public abstract Collection getChildren2();
   /**
    * @ejb.interface-method
    */
   public abstract void setChildren2(Collection children);

   // EntityBean implementation

   /**
    * @ejb.create-method
    */
   public Long ejbCreate(Long id, String firstName)
      throws CreateException
   {
      setId(id);
      setFirstName(firstName);
      return null;
   }

   public void ejbPostCreate(Long id, String firstName)
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
