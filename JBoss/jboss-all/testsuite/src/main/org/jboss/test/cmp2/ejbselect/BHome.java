package org.jboss.test.cmp2.ejbselect;

import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

public interface BHome extends EJBLocalHome {

    public B create(String id, String name, boolean bool)
         throws CreateException;

    public B findByPrimaryKey(String id) throws FinderException;

}
