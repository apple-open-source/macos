/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AddrUnitTestCase.java,v 1.4 2002/04/02 13:48:42 cgjung Exp $

package org.jboss.test.net.addr;

import samples.addr.Address;
import samples.addr.AddressBook;
import samples.addr.Phone;
import samples.addr.StateType;

import org.jboss.net.axis.AxisInvocationHandler;

import org.jboss.test.net.AxisTestCase;

import junit.framework.Test;

import java.net.URL;

/**
 * Tests remote accessibility of "ordinary" Axis services
 * @created 11. Oktober 2001
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.4 $
 */

public class AddrUnitTestCase extends AxisTestCase {
    
   protected String ADDRESS_END_POINT=END_POINT+"/AddressBook";
   
    // Constructors --------------------------------------------------
    public AddrUnitTestCase(String name)
   {
   	super(name);
   }
   
   /** prepared data */
   Address address;
   AddressBook book;
   
   /** sets up the test */
   public void setUp() throws Exception {
       super.setUp();

       interfaceMap.put(AddressBook.class,"http://net.jboss.org/samples/AddressBook");
       
       Phone phone=new Phone();
       
       phone.setExchange("(0)6897");
       phone.setNumber("6666");
       phone.setAreaCode(49);
       
       StateType state=StateType.fromString("TX");
       
       address=new Address();
       
       address.setStreetNum(42);
       
       address.setStreetName("Milky Way");
       
       address.setCity("Galactic City");
       
       address.setZip(2121);
       
       address.setState(state);
       
       address.setPhoneNumber(phone);
       
       book=(AddressBook) createAxisService(AddressBook.class,
                new URL(ADDRESS_END_POINT));
   }
       
   /** this is where the axis config is stored */
   protected String getAxisConfiguration() {
       return "addr/client/"+super.getAxisConfiguration();
   }
   
   /** routes an address to the server and tests the result */
   public void testAddress() throws Exception {
        book.addEntry ("George",address);
        assertEquals("Comparing addresses",address,book.
            getAddressFromName ("George"));
   }
   
   /** suite method */
   public static Test suite() throws Exception
   {
       try{
        return getAxisSetup(AddrUnitTestCase.class, "addr.wsr");
       } catch(Exception e) {
           e.printStackTrace();
           throw e;
       }
   }
}