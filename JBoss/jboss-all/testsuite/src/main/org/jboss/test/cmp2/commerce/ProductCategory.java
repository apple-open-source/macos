package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.EJBLocalObject; 

public interface ProductCategory extends EJBLocalObject  {
	public Long getId();
	
	public String getName();
	public void setName(String name);

   public Collection getProducts();
   public void setProducts(Collection products);
}
