package org.jboss.test.jmsra.bean;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;

public interface JMSSession extends EJBObject
{
   public void sendToQueueAndTopic()
      throws RemoteException;
}