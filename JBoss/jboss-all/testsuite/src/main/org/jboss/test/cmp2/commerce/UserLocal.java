package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.EJBLocalObject;
import javax.ejb.FinderException;

public interface UserLocal extends EJBLocalObject 
{
	public String getUserId();

	public String getUserName();
	public void setUserName(String name);

	public String getEmail();
	public void setEmail(String email);

	public boolean getSendSpam();
	public void setSendSpam(boolean sendSpam);

   public Collection getUserIds() throws FinderException;
}
