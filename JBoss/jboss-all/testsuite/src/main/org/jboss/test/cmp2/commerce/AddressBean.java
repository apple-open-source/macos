package org.jboss.test.cmp2.commerce;

import javax.ejb.CreateException;
import javax.ejb.EntityBean; 
import javax.ejb.EntityContext; 

import org.jboss.varia.autonumber.AutoNumberFactory;

public abstract class AddressBean implements EntityBean {
   transient private EntityContext ctx;

   public Long ejbCreate() throws CreateException {
      setId(new Long(AutoNumberFactory.getNextInteger("Address").longValue()));
      return null;
   }

   public void ejbPostCreate() {
   }

   public Long ejbCreate(Long id) throws CreateException {
      setId(id);
      return null;
   }

   public void ejbPostCreate(Long id) {
   }

   public abstract Long getId();
   public abstract void setId(Long id);

   public abstract String getStreet();
   public abstract void setStreet(String street);

   public abstract String getCity();
   public abstract void setCity(String city);

   public abstract String getState();
   public abstract void setState(String state);

   public abstract int getZip();
   public abstract void setZip(int zip);

   public abstract int getZipPlus4();
   public abstract void setZipPlus4(int zipPlus4);

   public void setEntityContext(EntityContext ctx) { this.ctx = ctx; }
   public void unsetEntityContext() { this.ctx = null; }
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbLoad() { }
   public void ejbStore() { }
   public void ejbRemove() { }
}
