/**
 * AddressBookSOAPBindingImpl.java
 *
 * This file was hand modified from the Emmitter generated code.
 */

package samples.addr;

import java.util.Hashtable;
import java.util.Map;

public class AddressBookSOAPBindingImpl implements AddressBook {
    private Map addresses = new Hashtable();

    public void addEntry(java.lang.String name, samples.addr.Address address) throws java.rmi.RemoteException {
        this.addresses.put(name, address);
    }
    public samples.addr.Address getAddressFromName(java.lang.String name) throws java.rmi.RemoteException {
        return (samples.addr.Address) this.addresses.get(name);
    }
}
