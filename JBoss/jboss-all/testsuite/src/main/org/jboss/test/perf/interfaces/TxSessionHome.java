package org.jboss.test.perf.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;
import java.rmi.RemoteException;

public interface TxSessionHome extends EJBHome
{
   public TxSession create() throws RemoteException, CreateException;
} 
