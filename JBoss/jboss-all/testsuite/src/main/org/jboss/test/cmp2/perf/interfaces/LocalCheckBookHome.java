package org.jboss.test.cmp2.perf.interfaces;

import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;
import javax.ejb.CreateException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface LocalCheckBookHome extends EJBLocalHome
{
   LocalCheckBook create(String account, double balance) throws CreateException;
   LocalCheckBook findByPrimaryKey(String key) throws FinderException;
}
