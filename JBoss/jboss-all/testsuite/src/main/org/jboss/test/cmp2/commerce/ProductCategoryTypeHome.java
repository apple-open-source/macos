package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

public interface ProductCategoryTypeHome extends EJBLocalHome
{
   public ProductCategoryType create() throws CreateException;

   public ProductCategoryType findByPrimaryKey(Long id) throws FinderException;
}
