package org.jboss.test.cmp2.relationship.oneToOneBidirectional;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

public interface AHome extends EJBLocalHome {
	public A create(Integer id) throws CreateException;
	public A findByPrimaryKey(Integer id) throws FinderException;
	public Collection findAll() throws FinderException;
}
