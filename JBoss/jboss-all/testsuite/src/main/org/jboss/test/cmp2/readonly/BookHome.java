package org.jboss.test.cmp2.readonly;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

public interface BookHome extends EJBLocalHome {
	public Book create(Integer id) throws CreateException;
	public Book findByPrimaryKey(Integer id) throws FinderException;
	public Book findByName(String name) throws FinderException;
	public Collection findAll() throws FinderException;
}
