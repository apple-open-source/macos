package org.jboss.test.cmp2.commerce;

import javax.ejb.EJBLocalObject;
import java.util.Collection;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public interface ProductCategoryType
   extends EJBLocalObject
{
   Long getId();

   String getName();

   void setName(String name);

   Collection getProductCategories();
}
