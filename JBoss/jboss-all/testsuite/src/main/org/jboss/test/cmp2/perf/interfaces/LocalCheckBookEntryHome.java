package org.jboss.test.cmp2.perf.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface LocalCheckBookEntryHome extends EJBLocalHome
{
   LocalCheckBookEntry create(Integer entryNo) throws CreateException;
   LocalCheckBookEntry findByPrimaryKey(Integer key) throws FinderException;
}
