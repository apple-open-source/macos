package org.jboss.test.deadlock.interfaces;


import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

public interface EnterpriseEntityLocalHome extends EJBLocalHome {

    public EnterpriseEntityLocal create(String name)
        throws CreateException;

    public EnterpriseEntityLocal findByPrimaryKey(String name)
        throws FinderException;

}
