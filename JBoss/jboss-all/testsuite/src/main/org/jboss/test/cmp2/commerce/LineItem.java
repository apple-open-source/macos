package org.jboss.test.cmp2.commerce;

import javax.ejb.EJBLocalObject; 

public interface LineItem extends EJBLocalObject {
	public Long getId();
	public void setId(Long id);
	
	public abstract Order getOrder();
	public abstract void setOrder(Order o);

	public abstract Product getProduct();
	public abstract void setProduct(Product p);
	
	public int getQuantity();
	public void setQuantity(int q);
	
	public boolean getShipped();
	public void setShipped(boolean shipped);
}
