package org.jboss.test.webservice.addr;

import java.util.Map;
import java.util.Hashtable;

import org.jboss.test.webservice.addr.Address;

/** This is the completed AddressBook implementation.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class AddressBookSOAPBindingImpl implements AddressBook
{
    private Map addresses = new Hashtable();

    public void addEntry(java.lang.String name, Address address)
       throws java.rmi.RemoteException
    {
        this.addresses.put(name, address);
    }

    public Address getAddressFromName(java.lang.String name)
       throws java.rmi.RemoteException
    {
        return (Address) this.addresses.get(name);
    }
}
