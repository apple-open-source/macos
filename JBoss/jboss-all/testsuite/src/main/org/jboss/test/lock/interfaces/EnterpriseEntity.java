package org.jboss.test.lock.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface EnterpriseEntity
   extends EJBObject
{
   void setField(String value) throws RemoteException;

   void setNextEntity(String nextBeanName) throws RemoteException;

   void setAndCopyField(String value) throws RemoteException;

   String getField() throws RemoteException;

   void sleep(long time) throws InterruptedException, RemoteException;
}
