package org.jboss.test.cmp2.perf.interfaces;

import java.util.Collection;
import javax.ejb.EJBLocalObject;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface LocalCheckBook extends EJBLocalObject
{
   Collection getCheckBookEntries();
   void setCheckBookEntries(Collection checkBookEntries);
   public double getBalance();
}
