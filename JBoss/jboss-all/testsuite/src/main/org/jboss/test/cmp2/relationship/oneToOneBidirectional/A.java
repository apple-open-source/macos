package org.jboss.test.cmp2.relationship.oneToOneBidirectional;

import javax.ejb.EJBLocalObject;

public interface A extends EJBLocalObject {
	public B getB();
	public void setB(B b);
}
