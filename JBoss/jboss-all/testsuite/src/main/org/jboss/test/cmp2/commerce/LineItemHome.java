package org.jboss.test.cmp2.commerce;

import java.util.Collection; 
import javax.ejb.CreateException; 
import javax.ejb.EJBLocalHome; 
import javax.ejb.FinderException; 

public interface LineItemHome extends EJBLocalHome {
	public LineItem create() throws CreateException;
	public LineItem create(Long id) throws CreateException;
	public LineItem create(Order order) throws CreateException;

	public LineItem findByPrimaryKey(Long id) throws FinderException;

	public Collection findAll() throws FinderException;
}
