package org.jboss.test.cmp2.readonly;

import javax.ejb.EJBLocalObject;

public interface Book extends EJBLocalObject {
   public Integer getId();
   public String getName();
   public void setName(String name);
   public String getIsbn();
   public void setIsbn(String isbn);
	public Publisher getPublisher();
	public void setPublisher(Publisher publisher);
}
