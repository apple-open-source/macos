package org.jboss.test.naming.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;

/**

@author  <a href="mailto:Adrian.Brock@HappeningTimes.com>Adrian Brock</a>
@version $Revision: 1.1.4.1 $
*/
public interface TestEjbLinkLocalHome extends EJBLocalHome
{
    public TestEjbLinkLocal create() throws CreateException;
}
