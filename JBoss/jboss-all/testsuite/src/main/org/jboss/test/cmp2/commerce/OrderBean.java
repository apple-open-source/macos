package org.jboss.test.cmp2.commerce;

import java.util.Collection; 
import java.util.Set;
import javax.ejb.CreateException;
import javax.ejb.EntityBean; 
import javax.ejb.EntityContext;
import javax.ejb.FinderException; 

import org.jboss.varia.autonumber.AutoNumberFactory;

public abstract class OrderBean implements EntityBean {
   transient private EntityContext ctx;

   public Long ejbCreate() throws CreateException {
      setOrdernumber(new Long(AutoNumberFactory.getNextInteger("Order").longValue()));
      return null;
   }

   public void ejbPostCreate() {
   }

   public Long ejbCreate(Long id) throws CreateException {
      setOrdernumber(id);
      return null;
   }

   public void ejbPostCreate(Long id) {
   }

   public abstract Long getOrdernumber();
   public abstract void setOrdernumber(Long ordernumber);
   
   public abstract Card getCreditCard();
   public abstract void setCreditCard(Card card);
   
   public abstract String getOrderStatus();
   public abstract void setOrderStatus(String orderStatus);

   public abstract Customer getCustomer();
   public abstract void setCustomer(Customer c);

   public abstract Collection getLineItems();
   public abstract void setLineItems(Collection lineItems);
   
   public abstract Address getShippingAddress();
   public abstract void setShippingAddress(Address shippingAddress);
   
   public abstract Address getBillingAddress();
   public abstract void setBillingAddress(Address billingAddress);

   public abstract Set ejbSelectOrdersShippedToCA() throws FinderException;
   public abstract Set ejbSelectOrdersShippedToCA2() throws FinderException;
   
   public abstract Collection ejbSelectOrderShipToStates()
         throws FinderException;
   public abstract Collection ejbSelectOrderShipToStates2()
         throws FinderException;

   public abstract Set ejbSelectAddressesInCA() throws FinderException;
   public abstract Set ejbSelectAddressesInCA2() throws FinderException;

   public Set getOrdersShippedToCA() throws FinderException {
      return ejbSelectOrdersShippedToCA();
   }
   
   public Set getOrdersShippedToCA2() throws FinderException {
      return ejbSelectOrdersShippedToCA2();
   }
   
   public Collection getStatesShipedTo() throws FinderException {
      return ejbSelectOrderShipToStates();
   }
   
   public Collection getStatesShipedTo2() throws FinderException {
      return ejbSelectOrderShipToStates2();
   }
   
   public Set getAddressesInCA() throws FinderException {
      return ejbSelectAddressesInCA();
   }
   
   public Set getAddressesInCA2() throws FinderException {
      return ejbSelectAddressesInCA2();
   }

   public Set ejbHomeGetStuff(String jbossQl, Object[] arguments) 
         throws FinderException {
      return ejbSelectGeneric(jbossQl, arguments);
   }
      
   public abstract Set ejbSelectGeneric(String jbossQl, Object[] arguments)
         throws FinderException;

   public void setEntityContext(EntityContext ctx) { this.ctx = ctx; }
   public void unsetEntityContext() { this.ctx = null; } 
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbLoad() { }
   public void ejbStore() { }
   public void ejbRemove() { }
}
