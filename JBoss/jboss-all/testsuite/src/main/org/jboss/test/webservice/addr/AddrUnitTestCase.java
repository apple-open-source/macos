/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AddrUnitTestCase.java,v 1.1.2.2 2003/09/30 21:30:34 starksm Exp $

package org.jboss.test.webservice.addr;

import org.jboss.test.webservice.AxisTestCase;
import org.jboss.test.webservice.addr.Address;
import org.jboss.test.webservice.addr.AddressBook;
import org.jboss.test.webservice.addr.Phone;
import org.jboss.test.webservice.addr.StateType;

import junit.framework.Test;

import java.net.URL;

/**
 * Tests remote accessibility of "ordinary" Axis services
 * @created 11. Oktober 2001
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.2 $
 */
public class AddrUnitTestCase extends AxisTestCase
{
   protected String ADDRESS_END_POINT = END_POINT + "/AddressBook";

   // Constructors --------------------------------------------------
   public AddrUnitTestCase(String name)
   {
      super(name);
   }

   /** prepared data */
   Address address;
   AddressBook book;

   /** sets up the test */
   public void setUp() throws Exception
   {
      super.setUp();

      interfaceMap.put(AddressBook.class, "http://net.jboss.org/samples/AddressBook");

      Phone phone = new Phone();
      phone.setExchange("(0)6897");
      phone.setNumber("6666");
      phone.setAreaCode(49);
      StateType state = StateType.fromString("TX");
      address = new Address();
      address.setStreetNum(42);
      address.setStreetName("Milky Way");
      address.setCity("Galactic City");
      address.setZip(2121);
      address.setState(state);
      address.setPhoneNumber(phone);

      book = (AddressBook) createAxisService(AddressBook.class,
         new URL(ADDRESS_END_POINT));
   }

   /** this is where the axis config is stored */
   protected String getAxisConfiguration()
   {
      return "webservice/addr/client/" + super.getAxisConfiguration();
   }

   /** routes an address to the server and tests the result */
   public void testAddress() throws Exception
   {
      book.addEntry("George", address);
      assertEquals("Comparing addresses", address, book.
         getAddressFromName("George"));
   }

   /** suite method */
   public static Test suite() throws Exception
   {
      try
      {
         return getAxisSetup(AddrUnitTestCase.class, "addr.wsr");
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw e;
      }
   }
}