/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkupdates.ejb;

import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.EntityEnterpriseContext;

import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.ejb.EntityContext;

/**
 * @ejb.bean
 *    name="ListItemEntity"
 *    type="CMP"
 *    view-type="local"
 *    cmp-version="2.x"
 *    primkey-field="itemId"
 * @ejb.util  generate="physical"
 * @ejb.persistence table-name="LIST_ITEM"
 * @jboss.read-ahead
 *    strategy="on-load"
 *    page-size="100"
 * @jboss.create-table "true"
 * @jboss.remove-table "true"
 * @ejb.finder
 *    signature="Collection findAll()"
 *    query="SELECT OBJECT(li) FROM ListItemEntity AS li"
 * @jboss.container-configuration name="custom container"
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class ListItemEntityBean implements EntityBean
{
   private EntityContext ctx;

   /**
    * @ejb.pk-field
    * @ejb.persistence column-name="ITEM_ID"
    */
   public abstract Integer getItemId();
   public abstract void setItemId(Integer itemId);

   /**
    * @ejb.interface-method
    * @ejb.persistence column-name="LIST_ID"
    */
   public abstract Integer getListIdFK();
   public abstract void setListIdFK(Integer listId);

   /**
    * @ejb.interface-method
    * @ejb.relation
    *    name="list-listItems"
    *    role-name="listItem-belongs_to-list"
    * @jboss.relation
    *    related-pk-field="id"
    *    fk-column="LIST_ID"
    */
   public abstract ListEntityLocal getList();
   /** @ejb.interface-method */
   public abstract void setList(ListEntityLocal list);

   /**
    * @ejb.create-method
    */
   public Integer ejbCreate(Integer itemId, Integer listId)
      throws CreateException
   {
      setItemId(itemId);
      setListIdFK(listId);
      return null;
   }

   public void ejbPostCreate(Integer itemId, Integer listId) {}

   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }

   public void unsetEntityContext()
   {
      this.ctx = null;
   }

   // Business methods

   /**
    * @ejb.interface-method
    */
   public void assertClean(JDBCEntityBridge bridge)
   {
      System.out.println("ctx: " + ctx.getClass().getName()
         + ",instanceof EntityEnterpriseContext: " + (ctx instanceof EntityEnterpriseContext));
      //bridge.getDirtyFields(ctx);
   }
}
