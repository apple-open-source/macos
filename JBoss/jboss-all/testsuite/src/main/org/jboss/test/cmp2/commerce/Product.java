package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.EJBLocalObject; 

public interface Product extends EJBLocalObject  {
	public Long getId();
	public void setId(Long id);
	
	public String getName();
	public void setName(String name);

	public String getType();
	public void setType(String type);

	public String getUnit();
	public void setUnit(String unit);

	public double getCostPerUnit();
	public void setCostPerUnit(double cost);

   public double getWeight();
   public void setWeight(double weight);

   public double getLength();
   public void setLength(double length);

   public double getGirth();
   public void setGirth(double girth);

   public Collection getProductCategories();
   public void setProductCategories(Collection productCategories);
}
