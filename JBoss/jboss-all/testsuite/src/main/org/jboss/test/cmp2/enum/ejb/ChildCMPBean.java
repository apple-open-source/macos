/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.enum.ejb;

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
 * @jboss.persistence
 *    datasource="${ds.name}"
 *    datasource-mapping="${ds.mapping}"
 *    create-table="${jboss.create.table}"
 *    remove-table="${jboss.remove.table}"
 * @ejb:transaction-type type="Container"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 + * @author <a href="mailto:gturner@unzane.com">Gerald Turner</a>
 */
public abstract class ChildCMPBean
   implements EntityBean
{
   // Attributes -----------------------------------------------
   private EntityContext ctx;

   // CMP accessors --------------------------------------------
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
    * @ejb.persistence column-name="COLOR_ID"
    */
   public abstract ColorEnum getColor();

   /**
    * @ejb.interface-method
    */
   public abstract void setColor(ColorEnum color);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence column-name="ANIMAL_ID"
    */
   public abstract AnimalEnum getAnimal();

   /**
    * @ejb.interface-method
    */
   public abstract void setAnimal(AnimalEnum animal);

   // EntityBean implementation -------------------------------------
   /**
    * @ejb.create-method
    * @throws CreateException
    */
   public Long ejbCreate(Long childId)
      throws CreateException
   {
      setId(childId);
      setColor(ColorEnum.RED);
      setAnimal(AnimalEnum.PENGUIN);
      return null;
   }

   public void ejbPostCreate(Long childId)
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

   public void ejbActivate()
   {
   }

   public void ejbLoad()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbRemove() throws RemoveException
   {
   }

   public void ejbStore()
   {
   }
}
