package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.CreateException; 
import javax.ejb.EJBLocalHome; 
import javax.ejb.FinderException; 

public interface AddressHome extends EJBLocalHome {
	public Address create() throws CreateException;
	public Address create(Long id) throws CreateException;

	public Address findByPrimaryKey(Long id) throws FinderException;

	public Collection findAll() throws FinderException;
}
