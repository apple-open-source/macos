package org.jboss.test.naming.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**

@author  <a href="mailto:Adrian.Brock@HappeningTimes.com>Adrian Brock</a>
@version $Revision: 1.1 $
*/
public interface TestEjbLinkHome extends EJBHome
{
    public TestEjbLink create() throws CreateException, RemoteException;
}
