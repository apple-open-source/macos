package org.jboss.test.cts.interfaces;



import java.rmi.RemoteException;
import javax.ejb.*;

public interface CtsCmp
   extends EJBObject
{
   /**
    * Method setPersonsName
    *
    *
    * @param personsName
    *
    * @throws RemoteException
    *
    */

   public void setPersonsName (String personsName)
      throws RemoteException;

   /**
    * Method getPersonsName
    *
    *
    * @return
    *
    * @throws RemoteException
    *
    */

   public String getPersonsName ()
      throws RemoteException;

}


/*------ Formatted by Jindent 3.23 Basic 1.0 --- http://www.jindent.de ------*/



