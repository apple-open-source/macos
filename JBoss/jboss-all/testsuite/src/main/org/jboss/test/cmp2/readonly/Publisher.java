package org.jboss.test.cmp2.readonly;

import java.util.Collection;
import javax.ejb.EJBLocalObject;

public interface Publisher extends EJBLocalObject {
   public Integer getId();
   public String getName();
   public void setName(String name);
	public Collection getBooks();
	public void setBooks(Collection books);
}
