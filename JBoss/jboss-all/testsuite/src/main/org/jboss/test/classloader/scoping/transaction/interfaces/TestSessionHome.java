package org.jboss.test.classloader.scoping.transaction.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

public interface TestSessionHome
   extends EJBHome
{
   public TestSession create() throws CreateException, RemoteException;
}
