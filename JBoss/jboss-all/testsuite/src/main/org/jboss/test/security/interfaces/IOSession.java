package org.jboss.test.security.interfaces;

import java.io.IOException;
import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.8.1 $
 */
public interface IOSession extends EJBObject
{
   public String read(String path) throws IOException, RemoteException;
   public String retryableRead(String path) throws IOException, RemoteException;
   public void write(String path) throws IOException, RemoteException;
}
