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
 *    primkey-field="id"
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
   public abstract Integer getId();
   public abstract void setId(Integer id);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract String getStringGroup1();
   /**
    * @ejb.interface-method
    */
   public abstract void setStringGroup1(String stringField);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract Integer getIntegerGroup1();
   /**
    * @ejb.interface-method
    */
   public abstract void setIntegerGroup1(Integer value);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract Double getDoubleGroup1();
   /**
    * @ejb.interface-method
    */
   public abstract void setDoubleGroup1(Double value);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract String getStringGroup2();
   /**
    * @ejb.interface-method
    */
   public abstract void setStringGroup2(String stringField);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract Integer getIntegerGroup2();
   /**
    * @ejb.interface-method
    */
   public abstract void setIntegerGroup2(Integer value);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract Double getDoubleGroup2();
   /**
    * @ejb.interface-method
    */
   public abstract void setDoubleGroup2(Double value);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract Long getVersionField();
   /**
    * @ejb.interface-method
    */
   public abstract void setVersionField(Long value);

   // EntityBean implementation -------------------------
   /**
    * @ejb.create-method
    */
   public Integer ejbCreate(Integer id,
                            String stringGroup1,
                            Integer integerGroup1,
                            Double doubleGroup1,
                            String stringGroup2,
                            Integer integerGroup2,
                            Double doubleGroup2)
      throws CreateException
   {
      setId(id);
      setStringGroup1(stringGroup1);
      setIntegerGroup1(integerGroup1);
      setDoubleGroup1(doubleGroup1);
      setStringGroup2(stringGroup2);
      setIntegerGroup2(integerGroup2);
      setDoubleGroup2(doubleGroup2);
      return null;
   }

   public void ejbPostCreate(Integer id,
                             String stringGroup1,
                             Integer integerGroup1,
                             Double doubleGroup1,
                             String stringGroup2,
                             Integer integerGroup2,
                             Double doubleGroup2) {}

   public void ejbRemove() throws RemoveException
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
