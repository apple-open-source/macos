package org.jboss.test.jmsra.bean;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;


public interface Publisher extends EJBObject {
    public static final String JMS_MESSAGE_NR = "MESSAGE_NR";
    public void simple(int messageNr) throws RemoteException;
    public void simpleFail(int messageNr) throws RemoteException;   
    public void beanOk(int messageNr) throws RemoteException;    
    public void beanError(int messageNr) throws RemoteException;
}
