package org.jboss.test.naming.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**

@author  Scott_Stark@displayscape.com
@version $Revision: 1.1 $
*/
public interface TestENCHome2 extends EJBHome
{
    public TestENC create() throws CreateException, RemoteException;
}
