package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EntityBean; 
import javax.ejb.EntityContext; 

import org.jboss.varia.autonumber.AutoNumberFactory;

public abstract class ProductBean implements EntityBean {
	transient private EntityContext ctx;

	public Long ejbCreate() throws CreateException { 
		setId(new Long(AutoNumberFactory.getNextInteger("Product").longValue()));
		return null;
	}

	public void ejbPostCreate() { }

	public abstract Long getId();
	public abstract void setId(Long id);
	
	public abstract String getName();
	public abstract void setName(String name);

	public abstract String getType();
	public abstract void setType(String type);

	public abstract String getUnit();
	public abstract void setUnit(String unit);

	public abstract double getCostPerUnit();
	public abstract void setCostPerUnit(double cost);

   public abstract double getWeight();
   public abstract void setWeight(double weight);

   public abstract double getLength();
   public abstract void setLength(double length);

   public abstract double getGirth();
   public abstract void setGirth(double girth);

   public abstract Collection getProductCategories();
   public abstract void setProductCategories(Collection productCategories);
	
	public void setEntityContext(EntityContext ctx) { this.ctx = ctx; }
	public void unsetEntityContext() {
		this.ctx = null;
	}
	public void ejbActivate() { }
	public void ejbPassivate() { }
	public void ejbLoad() { }
	public void ejbStore() { }
	public void ejbRemove() { }
}
