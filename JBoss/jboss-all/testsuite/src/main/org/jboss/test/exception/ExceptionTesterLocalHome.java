package org.jboss.test.exception;

import java.rmi.RemoteException;
import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;

public interface ExceptionTesterLocalHome extends EJBLocalHome
{
   ExceptionTesterLocal create() throws CreateException;
}
