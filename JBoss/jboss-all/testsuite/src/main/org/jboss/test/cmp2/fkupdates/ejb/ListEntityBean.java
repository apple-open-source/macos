/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkupdates.ejb;

import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.ejb.EntityContext;

/**
 * @ejb.bean
 *    name="ListEntity"
 *    type="CMP"
 *    view-type="local"
 *    cmp-version="2.x"
 * @ejb.util generate="physical"
 * @ejb.persistence table-name="LIST"
 * @jboss.read-ahead
 *    strategy="on-load"
 *    page-size="50"
 * @jboss.create-table "true"
 * @jboss.remove-table "true"
 * @ejb.finder
 *    signature="Collection findAll()"
 *    query="SELECT OBJECT(list) FROM ListEntity AS list"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class ListEntityBean implements EntityBean
{
   private EntityContext ctx;

   /**
    * @ejb.pk-field
    * @ejb.persistence column-name="ID"
    * @ejb.interface-method
    */
   public abstract Integer getId();
   public abstract void setId(Integer id);

   /**
    * @ejb.persistence column-name="NAME"
    * @ejb.interface-method
    */
   public abstract String getName();
   public abstract void setName(String v);

   /**
    * @ejb.facade-method
    * @ejb.interface-method
    * @ejb.relation
    *    name="list-listItems"
    *    role-name="list-has-listItems"
    *    target-ejb="ListItemEntity"
    */
   public abstract java.util.Collection getItems();
   /** @ejb.interface-method */
   public abstract void setItems(java.util.Collection items);

   /**
    * @ejb.create-method
    */
   public ListEntityPK ejbCreate(Integer id, String name)
      throws CreateException
   {
      setId(id);
      setName(name);
      return null;
   }

   public void ejbPostCreate(Integer id, String name) {}

   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }

   public void unsetEntityContext()
   {
      this.ctx = null;
   }
}
