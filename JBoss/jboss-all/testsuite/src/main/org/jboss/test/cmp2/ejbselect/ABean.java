package org.jboss.test.cmp2.ejbselect;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;
import java.util.Collection;
import java.util.Iterator;

public abstract class ABean implements EntityBean {
   private EntityContext ctx;

   public ABean() {}

   public String ejbCreate(String id) throws CreateException {
      setId(id);
      return null;
   }

   public void ejbPostCreate(String id) { }

   public abstract String getId();

   public abstract void setId(String id);

   public abstract Collection getBs();

   public abstract void setBs(Collection Bs);

   public abstract Collection ejbSelectSomeBs(A a) throws FinderException;

   public Collection getSomeBs() throws FinderException {
      return ejbSelectSomeBs((A)ctx.getEJBLocalObject());
   }

   public Collection ejbHomeGetSomeBs(A a) throws FinderException {
      return ejbSelectSomeBs(a);
   }

   public abstract Collection ejbSelectSomeBsDeclaredSQL(A a) throws FinderException;

   public Collection getSomeBsDeclaredSQL() throws FinderException {
      return ejbSelectSomeBsDeclaredSQL((A)ctx.getEJBLocalObject());
   }

   public Collection ejbHomeGetSomeBsDeclaredSQL(A a) throws FinderException {
      return ejbSelectSomeBsDeclaredSQL(a);
   }

   public abstract Collection ejbSelectAWithBs() throws FinderException;

   public Collection getAWithBs() throws FinderException {
      return ejbSelectAWithBs();
   }

   public void setEntityContext(EntityContext ctx) { 
      this.ctx = ctx;
   }
    
   public void unsetEntityContext() { 
      this.ctx = null;
   }

   public void ejbRemove() { }
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbLoad() { }
   public void ejbStore() { }
}
