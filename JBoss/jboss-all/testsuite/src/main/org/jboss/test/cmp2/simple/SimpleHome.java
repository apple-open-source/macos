package org.jboss.test.cmp2.simple;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

public interface SimpleHome extends EJBLocalHome {
	public Simple create(String id) throws CreateException;
	public Simple findByPrimaryKey(String id) throws FinderException;
	public Collection findAll() throws FinderException;
	public Collection findWithByteArray(byte[] bytes) throws FinderException;
	public Collection selectDynamic(String jbossQl, Object[] args) 
         throws FinderException;
	public Collection selectValueClass() throws FinderException;
}
