package org.jboss.test.cmp2.ejbselect;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;
import javax.ejb.RemoveException;

/**
 * @ejb.bean
 *    name="B"
 *    type="CMP"
 *    cmp-version="2.x"
 *    view-type="local"
 *    primkey-field="id"
 * @ejb.pk generate="true"
 * @ejb.util  generate="physical"
 * @ejb.persistence  table-name="TEST_B"
 * @jboss.persistence
 *    create-table="true"
 *    remove-table="true"
 *
 * @jboss.query
 *    signature="Collection ejbSelectDynamic(java.lang.String ql, java.lang.Object[] params)"
 *    dynamic="true"
 *
 * @author others + <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public abstract class BBean implements EntityBean
{
   /**
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    * @ejb.persistence  column-name="ID"
    */
   public abstract String getId();
   public abstract void setId(String id);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract String getName();
   /**
    * @ejb.interface-method
    */
   public abstract void setName(String name);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract boolean getBool();
   /**
    * @ejb.interface-method
    */
   public abstract void setBool(boolean bool);

   /**
    * @ejb.persistent-field
    * @ejb.interface-method
    */
   public abstract long getLongField();
   /**
    * @ejb.interface-method
    */
   public abstract void setLongField(long value);

   /**
    * @ejb.relation
    *    name="A-B"
    *    role-name="B-has-an-A"
    *    cascade-delete="true"
    * @jboss.relation
    *    related-pk-field="id"
    *    fk-column="A_ID"
    *
    * @ejb.interface-method
    */
   public abstract ALocal getA();
   /**
    * @ejb.interface-method
    */
   public abstract void setA(ALocal a);

   // ejbSelect methods

   /**
    * @ejb.select query="SELECT OBJECT(b) FROM B AS b WHERE b.bool = TRUE"
    */
   public abstract Collection ejbSelectTrue() throws FinderException;

   /**
    * @ejb.select query="SELECT OBJECT(b) FROM B AS b WHERE b.bool = FALSE"
    */
   public abstract Collection ejbSelectFalse() throws FinderException;

   /**
    * Dynamic QL
    * @ejb.select query=""
    */
   public abstract Collection ejbSelectDynamic(String ql, Object[] params) throws FinderException;

   // Interface methods

   /**
    * @ejb.interface-method
    */
   public Collection getTrue() throws FinderException
   {
      return ejbSelectTrue();
   }

   /**
    * @ejb.interface-method
    */
   public Collection getFalse() throws FinderException
   {
      return ejbSelectFalse();
   }

   // Home methods

   /**
    * @ejb.home-method
    */
   public Collection ejbHomeSelectDynamic(String ql, Object[] params) throws FinderException
   {
      return ejbSelectDynamic(ql, params);
   }

   /**
    * @ejb.create-method
    */
   public String ejbCreate(String id, String name, boolean bool)
      throws CreateException
   {
      setId(id);
      setName(name);
      setBool(bool);
      return null;
   }

   public void ejbPostCreate(String id, String name, boolean bool) {}

   public void setEntityContext(EntityContext context) {}
   public void unsetEntityContext() {}
   public void ejbRemove() throws RemoveException {}
   public void ejbActivate() {}
   public void ejbPassivate() {}
   public void ejbLoad() {}
   public void ejbStore() {}
}
