package org.jboss.test.cmp2.commerce;

import java.util.Collection; 
import javax.ejb.CreateException; 
import javax.ejb.EJBLocalHome; 
import javax.ejb.FinderException; 

public interface ProductHome extends EJBLocalHome {
	public Product create() throws CreateException;

	public Product findByPrimaryKey(Long id) throws FinderException;

	public Collection findByName(String name) throws FinderException;
	
	public Collection findAll() throws FinderException;
}
