package org.jboss.test.cmp2.ejbselect;

import javax.ejb.EJBLocalObject;
import javax.ejb.FinderException;
import java.util.Collection;

public interface A extends EJBLocalObject {
   public String getId();

   public Collection getSomeBs() throws FinderException;
   public Collection getSomeBsDeclaredSQL() throws FinderException;

   public Collection getBs();

   public void setBs(Collection Bs);

   public Collection getAWithBs() throws FinderException;
}
