package org.jboss.test.cmp2.readonly;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

public interface PublisherHome extends EJBLocalHome {
	public Publisher create(Integer id) throws CreateException;
	public Publisher findByPrimaryKey(Integer id) throws FinderException;
	public Publisher findByName(String name) throws FinderException;
	public Collection findAll() throws FinderException;
}
