package org.jboss.test.cmp2.ejbselect;

import java.util.Collection;
import javax.ejb.EJBLocalObject;
import javax.ejb.FinderException;

public interface B extends EJBLocalObject {

    public String getId();

    public String getName();

    public A getA();
    public void setA(A a);

    public boolean getBool();
    public void setBool(boolean b);

    public Collection getTrue() throws FinderException;
    public Collection getFalse() throws FinderException;
}
