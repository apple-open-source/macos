/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.cmp2.optimisticlock.ejb;

import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.ejb.EntityContext;

/**
 * @ejb.bean
 *    name="CmpEntity"
 *    local-jndi-name="local/CmpEntityBean"
 *    view-type="local"
 *    type="CMP"
 *    reentrant="false"
 *    cmp-version="2.x"
 *    primkey-field="integerField"
 *
 * @jboss.create-table "true"
 * @jboss.remove-table "false"
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 */
public abstract class CmpEntityBean
   implements EntityBean
{
   // Attributes ----------------------------------------
   private EntityContext ctx;

   // CMP accessors -------------------------------------
   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract Integer getIntegerField();
   public abstract void setIntegerField(Integer integerField);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract String getStringField();
   /**
    * @ejb.interface-method
    */
   public abstract void setStringField(String stringField);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract java.util.Date getDateField();
   /**
    * @ejb.interface-method
    */
   public abstract void setDateField(java.util.Date dateField);

   // EntityBean implementation -------------------------
   /**
    * @ejb.create-method
    */
   public Integer ejbCreate(Integer integerField,
                            String stringField,
                            java.util.Date dateField)
      throws CreateException
   {
      setIntegerField(integerField);
      setStringField(stringField);
      setDateField(dateField);
      return null;
   }

   public void ejbPostCreate(Integer integerField,
                             String stringField,
                             java.util.Date dateField)
   {}

   public void ejbRemove()
      throws RemoveException
   {}

   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }

   public void unsetEntityContext()
   {
      this.ctx = null;
   }

   public void ejbActivate() {}
   public void ejbPassivate() {}
   public void ejbLoad() {}
   public void ejbStore() {}
}
