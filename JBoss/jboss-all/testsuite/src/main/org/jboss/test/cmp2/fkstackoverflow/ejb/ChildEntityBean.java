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


/**
 * @ejb.bean
 *    name="Child"
 *    type="CMP"
 *    cmp-version="2.x"
 *    view-type="local"
 *    reentrant="false"
 *    primkey-field="id"
 * @ejb.pk generate="false"
 * @ejb.util  generate="physical"
 * @ejb.persistence  table-name="CHILD"
 * @ejb:transaction  type="Required"
 * @ejb:transaction-type  type="Container"
 * @jboss.persistence
 *    create-table="true"
 *    remove-table="true"
 */
public abstract class ChildEntityBean
   implements EntityBean
{
   Category log = Category.getInstance(ChildEntityBean.class);
   private EntityContext ctx;

   // CMP accessors

   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence  column-name="CHILD_ID"
    */
   public abstract Long getId();
   public abstract void setId(Long id);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="FIRST_NAME"
    */
   public abstract String getFirstName();
   public abstract void setFirstName(String name);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="PARENT_ID"
    */
   public abstract Long getSimpleParentId();
   /**
    * @ejb.interface-method
    */
   public abstract void setSimpleParentId(Long parentId);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="COMPLEXPARENT_ID1"
    */
   public abstract Long getComplexParentId1();
   /**
    * @ejb.interface-method
    */
   public abstract void setComplexParentId1(Long parentId);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="COMPLEXPARENT_ID2"
    */
   public abstract Long getComplexParentId2();
   /**
    * @ejb.interface-method
    */
   public abstract void setComplexParentId2(Long parentId);

   // CMR

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="parent-children-simple1"
    *    role-name="child-has-parent"
    *    cascade-delete="false"
    * @jboss.relation
    *    fk-column="PARENT_ID"
    *    related-pk-field="id"
    */
   public abstract SimpleParentLocal getSimpleParent1();
   /**
    * @ejb.interface-method
    */
   public abstract void setSimpleParent1(SimpleParentLocal parent);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="parent-children-simple2"
    *    role-name="child-has-parent"
    *    cascade-delete="false"
    * @jboss.relation
    *    fk-column="PARENT_ID"
    *    related-pk-field="id"
    */
   public abstract SimpleParentLocal getSimpleParent2();
   /**
    * @ejb.interface-method
    */
   public abstract void setSimpleParent2(SimpleParentLocal parent);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="parent-children-complex1"
    *    role-name="child-has-parent"
    *    cascade-delete="false"
    * @jboss.relation
    *    fk-column="COMPLEXPARENT_ID1"
    *    related-pk-field="id1"
    * @jboss.relation
    *    fk-column="COMPLEXPARENT_ID2"
    *    related-pk-field="id2"
    */
   public abstract ComplexParentLocal getComplexParent1();
   /**
    * @ejb.interface-method
    */
   public abstract void setComplexParent1(ComplexParentLocal parent);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="parent-children-complex2"
    *    role-name="child-has-parent"
    *    cascade-delete="false"
    * @jboss.relation
    *    fk-column="COMPLEXPARENT_ID1"
    *    related-pk-field="id1"
    * @jboss.relation
    *    fk-column="COMPLEXPARENT_ID2"
    *    related-pk-field="id2"
    */
   public abstract ComplexParentLocal getComplexParent2();
   /**
    * @ejb.interface-method
    */
   public abstract void setComplexParent2(ComplexParentLocal parent);

   // EntityBean implementation

   /**
    * @ejb.create-method
    * @throws CreateException
    */
   public Long ejbCreate(Long childId, String firstName, Long parentId)
      throws CreateException
   {
      setId(childId);
      setFirstName(firstName);
      setSimpleParentId(parentId);
      return null;
   }

   public void ejbPostCreate(Long childId, String firstName, Long parentId)
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
