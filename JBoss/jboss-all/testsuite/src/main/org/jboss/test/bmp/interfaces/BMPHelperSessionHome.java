package org.jboss.test.bmp.interfaces;


import java.rmi.RemoteException;

import javax.ejb.*;


public interface BMPHelperSessionHome
extends EJBHome
{
   public BMPHelperSession create () throws CreateException, RemoteException;
}
