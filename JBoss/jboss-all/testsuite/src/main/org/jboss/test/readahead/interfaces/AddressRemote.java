package org.jboss.test.readahead.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 * Remote interface for one of the entities used in read-ahead finder tests
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: AddressRemote.java,v 1.1 2001/06/30 04:38:05 danch Exp $
 * 
 * Revision:
 */
public interface AddressRemote extends EJBObject {
   public java.lang.String getZip() throws RemoteException;
   public void setZip(java.lang.String newZip) throws RemoteException;
   public java.lang.String getState() throws RemoteException;
   public void setState(java.lang.String newState) throws RemoteException;
   public java.lang.String getCity() throws RemoteException;
   public void setCity(java.lang.String newCity) throws RemoteException;
   public void setAddress(java.lang.String newAddress) throws RemoteException;
   public java.lang.String getAddress() throws RemoteException;
   public java.lang.String getAddressId() throws RemoteException;
   public java.lang.String getKey() throws RemoteException;
   public void setAddressId(java.lang.String newAddressId) throws RemoteException;
}