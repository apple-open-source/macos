package org.jboss.test.readahead.interfaces;

import java.util.Collection;
import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 * Home interface for one of the entities used in read-ahead finder tests
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: AddressHome.java,v 1.1 2001/06/30 04:38:05 danch Exp $
 * 
 * Revision:
 */
public interface AddressHome extends EJBHome {
   public AddressRemote create(String key, String addressId, String address, 
                               String city, String state, String zip) throws RemoteException, CreateException;
   public AddressRemote findByPrimaryKey(AddressPK primaryKey) throws RemoteException, FinderException;
   public Collection findByKey(String key) throws RemoteException, FinderException;
   public Collection findByCity(String city) throws RemoteException, FinderException;
}