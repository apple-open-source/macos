package org.jboss.test.jmsra.bean;

import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import java.rmi.RemoteException;

public interface JMSSessionHome extends EJBHome
{
   public JMSSession create()
      throws RemoteException, CreateException;
}