package org.jboss.test.invokers.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatelessSession extends EJBObject
{
   public SimpleBMP getBMP(int id) throws RemoteException;
}
