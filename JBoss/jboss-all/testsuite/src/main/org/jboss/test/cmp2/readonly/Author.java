package org.jboss.test.cmp2.readonly;

import javax.ejb.EJBLocalObject;

public interface Author extends EJBLocalObject {
   public Integer getId();
   public String getName();
   public void setName(String name);
}
