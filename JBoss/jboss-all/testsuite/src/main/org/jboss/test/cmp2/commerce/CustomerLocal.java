package org.jboss.test.cmp2.commerce;

import java.rmi.RemoteException; 
import java.util.Collection;
import javax.ejb.EJBLocalObject; 

public interface CustomerLocal extends EJBLocalObject {
	public Long getId();
	
	public String getName();
	
	public void setName(String name);
	
	public UserLocal getUserLocal();
	
	public void setUserLocal(UserLocal user);

   public Collection getAddresses();
}
