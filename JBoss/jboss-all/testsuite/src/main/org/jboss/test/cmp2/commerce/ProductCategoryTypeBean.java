package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.CreateException;

import org.jboss.varia.autonumber.AutoNumberFactory;

public abstract class ProductCategoryTypeBean implements EntityBean, ProductCategoryType
{
   transient private EntityContext ctx;

   public Long ejbCreate() throws CreateException
   {
      setId(new Long(AutoNumberFactory.getNextInteger("ProductCategoryType").longValue()));
      return null;
   }

   public void ejbPostCreate()
   {
   }

   public abstract void setId(Long id);

   public abstract void setProductCategories(Collection girth);

   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }

   public void unsetEntityContext()
   {
      this.ctx = null;
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbLoad()
   {
   }

   public void ejbStore()
   {
   }

   public void ejbRemove()
   {
   }
}
