package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Set;
import javax.ejb.EJBLocalObject;
import javax.ejb.FinderException;

public interface Order extends EJBLocalObject {
	public Long getOrdernumber();
	
   public Card getCreditCard();
   public void setCreditCard(Card card);
   
	public String getOrderStatus();
	public void setOrderStatus(String orderStatus);
	
	public Address getShippingAddress();
	public void setShippingAddress(Address address);

	public Address getBillingAddress();
	public void setBillingAddress(Address address);

   public Collection getLineItems();
   public void setLineItems(Collection lineItems);
	
	public Set getOrdersShippedToCA() throws FinderException;
	public Set getOrdersShippedToCA2() throws FinderException;
	
	public Collection getStatesShipedTo() throws FinderException;
	public Collection getStatesShipedTo2() throws FinderException;

	public Set getAddressesInCA() throws FinderException;
	public Set getAddressesInCA2() throws FinderException;
}
