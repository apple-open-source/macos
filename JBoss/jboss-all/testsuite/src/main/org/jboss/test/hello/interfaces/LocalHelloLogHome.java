package org.jboss.test.hello.interfaces;

import javax.ejb.FinderException;
import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface LocalHelloLogHome extends EJBLocalHome
{
   public LocalHelloLog create(String msg)
      throws CreateException;
   public LocalHelloLog findByPrimaryKey(String key)
      throws FinderException;
}
