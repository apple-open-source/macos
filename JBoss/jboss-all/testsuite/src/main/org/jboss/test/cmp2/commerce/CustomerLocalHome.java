package org.jboss.test.cmp2.commerce;

import java.util.Collection; 
import javax.ejb.CreateException; 
import javax.ejb.EJBLocalHome; 
import javax.ejb.FinderException; 

public interface CustomerLocalHome extends EJBLocalHome {
	public CustomerLocal create() throws CreateException;

	public CustomerLocal findByPrimaryKey(Long id) throws FinderException;

	public Collection findByName(String name) throws FinderException;
	
	public Collection findAll() throws FinderException;
}
