package org.jboss.test.cmp2.relationship.oneToManyUnidirectional;

import java.util.Collection;
import javax.ejb.EJBLocalObject;

public interface A extends EJBLocalObject {
   public Integer getId();
	public Collection getB();
	public void setB(Collection b);
}
