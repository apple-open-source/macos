package org.jboss.test.exception;

import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;

public interface ExceptionTesterHome extends EJBHome
{
   ExceptionTester create() throws CreateException, RemoteException;
}
