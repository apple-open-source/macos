package org.jboss.test.cmp2.commerce;

import javax.ejb.EJBLocalObject; 

public interface Address extends EJBLocalObject {
	public Long getId();
	
	public String getStreet();
	public void setStreet(String street);
	
	public String getCity();
	public void setCity(String city);

	public String getState();
	public void setState(String state);

   public int getZip();
   public void setZip(int zip);

   public int getZipPlus4();
   public void setZipPlus4(int zipPlus4);
}
