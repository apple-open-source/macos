package org.jboss.test.jmsra.bean;

import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;


public interface PublisherHome extends EJBHome {
    Publisher create() throws RemoteException, CreateException;
}
