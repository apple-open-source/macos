package org.jboss.test.cmp2.relationship.manyToManyBidirectional;

import java.util.Collection;
import javax.ejb.EJBLocalObject;

public interface B extends EJBLocalObject {
	public Collection getA();
	public void setA(Collection a);
}
