package org.jboss.test.cmp2.ejbselect;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;
import java.rmi.RemoteException;

public abstract class BBean implements EntityBean {

   public BBean() {}

   public String ejbCreate(String id, String name, boolean bool) 
         throws CreateException {
     setId(id);
     setName(name);
     setBool(bool);
     return null;
   }

   public void ejbPostCreate(String id, String name, boolean bool) { }

   public abstract String getId();
   public abstract void setId(String id);

   public abstract String getName();
   public abstract void setName(String name);

   public abstract A getA();
   public abstract void setA(A a);

   public abstract boolean getBool();
   public abstract void setBool(boolean bool);

   public abstract Collection ejbSelectTrue() throws FinderException;
   public Collection getTrue() throws FinderException {
     return ejbSelectTrue();
   }

   public abstract Collection ejbSelectFalse() throws FinderException;
   public Collection getFalse() throws FinderException {
     return ejbSelectFalse();
   }

   public void setEntityContext(EntityContext context) { }
   public void unsetEntityContext() { }
   public void ejbRemove() { }
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbLoad() { }
   public void ejbStore() { }

}
