package org.jboss.test.cmp2.readonly;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

public interface AuthorHome extends EJBLocalHome {
	public Author create(Integer id) throws CreateException;
	public Author findByPrimaryKey(Integer id) throws FinderException;
	public Author findByName(String name) throws FinderException;
	public Collection findAll() throws FinderException;
}
