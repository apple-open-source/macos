package org.jboss.test.cmp2.ejbselect;

import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.util.Collection;

public interface AHome extends EJBLocalHome {

    public A create(String id) throws CreateException;

    public A findByPrimaryKey(String id) throws FinderException;

    public Collection getSomeBs(A a) throws FinderException;
    public Collection getSomeBsDeclaredSQL(A a) throws FinderException;
}
