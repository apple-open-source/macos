package org.jboss.test.bench.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.EJBHome;

public interface ComplexEntityHome extends EJBHome {
   
   public ComplexEntity create(boolean aBoolean, int anInt, long aLong, double aDouble, String aString) 
      throws RemoteException, CreateException;

   public ComplexEntity findByPrimaryKey(AComplexPK aComplexPK) 
      throws RemoteException, FinderException;
}
