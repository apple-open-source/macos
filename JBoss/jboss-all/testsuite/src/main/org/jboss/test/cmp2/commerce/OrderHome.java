package org.jboss.test.cmp2.commerce;

import java.util.Collection; 
import java.util.Set; 
import javax.ejb.CreateException; 
import javax.ejb.EJBLocalHome; 
import javax.ejb.FinderException; 

public interface OrderHome extends EJBLocalHome {
	public Order create() throws CreateException;
	public Order create(Long id) throws CreateException;

	public Order findByPrimaryKey(Long ordernumber) throws FinderException;

	public Collection findByStatus(String status) throws FinderException;
	
	public Collection findAll() throws FinderException;

	public Collection findDoubleJoin(int a, int b) throws FinderException;

   public Collection findWithLimitOffset(int offset, int limit) throws FinderException;

   public Set getStuff(String jbossQl, Object[] arguments)
         throws FinderException;
}
