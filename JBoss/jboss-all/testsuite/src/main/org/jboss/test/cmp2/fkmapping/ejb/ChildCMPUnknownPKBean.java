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
 *    name="ChildUPK"
 *    type="CMP"
 *    cmp-version="2.x"
 *    view-type="local"
 *    reentrant="false"
 * @ejb.pk
 *    class="java.lang.Object"
 *    generate="false"
 * @ejb.util  generate="physical"
 * @ejb.persistence  table-name="CHILD_UPK"
 * @jboss.persistence
 *    create-table="true"
 *    remove-table="true"
 * @ejb:transaction-type  type="Container"
 * @jboss.container-configuration name="INSERT after ejbPostCreate Container"
 *
 * @jboss.unknown-pk class="java.lang.String"
 * @jboss.entity-command name="key-generator"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class ChildCMPUnknownPKBean
   implements EntityBean
{
   // Attributes -----------------------------------------------
   private EntityContext ctx;

   // CMP accessors --------------------------------------------
   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="FIRST_NAME"
    */
   public abstract String getFirstName();
   public abstract void setFirstName(String name);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="Father-Child-upk"
    *    role-name="Child-has-Father"
    *    target-ejb="Parent"
    *    target-role-name="Father-has-Child"
    *    target-multiple="yes"
    *    cascade-delete="no"
    * @jboss.relation
    *    related-pk-field="id"
    *    fk-column="FATHER_ID"
    * @jboss.relation
    *    related-pk-field="firstName"
    *    fk-column="FATHER_NAME"
    */
   public abstract ParentLocal getFather();
   /**
    * @ejb.interface-method
    */
   public abstract void setFather(ParentLocal parent);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="Mother-Child-upk"
    *    role-name="Child-has-Mother"
    *    target-ejb="Parent"
    *    target-role-name="Mother-has-Child"
    *    target-multiple="yes"
    *    cascade-delete="no"
    * @jboss.relation
    *    related-pk-field="id"
    *    fk-column="MOTHER_ID"
    * @jboss.relation
    *    related-pk-field="firstName"
    *    fk-column="MOTHER_NAME"
    */
   public abstract ParentLocal getMother();
   /**
    * @ejb.interface-method
    */
   public abstract void setMother(ParentLocal parent);

   // EntityBean implementation -------------------------------------
   /**
    * @ejb.create-method
    */
   public Object ejbCreate(String firstName)
      throws CreateException
   {
      setFirstName(firstName);
      return null;
   }

   public void ejbPostCreate(Long id, String firstName) {}

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
