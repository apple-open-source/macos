package org.jboss.test.cmp2.commerce;

import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;
import java.util.Collection;

public interface UserLocalHome extends EJBLocalHome
{
	public UserLocal create(String userId) throws CreateException;

	public UserLocal findByPrimaryKey(String id) throws FinderException;

	public UserLocal findByUserName(String userName) throws FinderException;

	public Collection findAll() throws FinderException;
}

