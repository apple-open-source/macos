package org.jboss.test.cmp2.relationship.oneToOneBidirectional;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

public interface BHome extends EJBLocalHome {
	public B create(Integer id) throws CreateException;
	public B findByPrimaryKey(Integer id) throws FinderException;
	public Collection findAll() throws FinderException;
}
