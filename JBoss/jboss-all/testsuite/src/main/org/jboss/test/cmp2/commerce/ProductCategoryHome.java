package org.jboss.test.cmp2.commerce;

import java.util.Collection; 
import javax.ejb.CreateException; 
import javax.ejb.EJBLocalHome; 
import javax.ejb.FinderException; 

public interface ProductCategoryHome extends EJBLocalHome {
	public ProductCategory create() throws CreateException;

	public ProductCategory findByPrimaryKey(Long id) throws FinderException;

	public Collection findAll() throws FinderException;
}
