/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: StoreUnitTestCase.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store;

import java.net.URL;

import junit.framework.Test;

import org.jboss.test.webservice.AxisTestCase;
import org.jboss.test.webservice.store.Address;

/**
 * Tests remote accessibility of store objects
 * @created 22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class StoreUnitTestCase extends AxisTestCase
{

   protected String ITEM_END_POINT = END_POINT + "/ItemService";
   protected String BP_END_POINT = END_POINT + "/BusinessPartnerService";
   protected String NAMESPACE = "http://net.jboss.org/samples/store";

   // Constructors --------------------------------------------------
   public StoreUnitTestCase(String name)
   {
      super(name);
   }

   /** the session bean with which we interact */
   Item.Service itemService;
   BusinessPartner.Service businessPartnerService;

   /** setup the bean */
   public void setUp() throws Exception
   {
      super.setUp();
      interfaceMap.put(Item.Service.class, NAMESPACE);
      interfaceMap.put(BusinessPartner.Service.class, NAMESPACE);
      itemService = (Item.Service) createAxisService(Item.Service.class,
         new URL(ITEM_END_POINT));
      businessPartnerService = (BusinessPartner.Service)
         createAxisService(BusinessPartner.Service.class,
            new URL(BP_END_POINT));
      itemService.create("Stahlblech, verzinkt, 10mm");
      itemService.create("Tragrolle, rund, 0.7cm");
      itemService.create("Muffe, schwarz");
      itemService.create("Muffe, rot");
      itemService.create("Holzschraube, 2mm");
      itemService.create("Steinbohrer, 4mm");
      itemService.create("Dübel, 6mm");
      itemService.create("Mutter, klein");
      itemService.create("Schraubverschluss, Kunststoff");
      BusinessPartner newBp =
         businessPartnerService.create("Marc Fleury");
      Address newAddress = new Address();
      newAddress.setStreetName("Alien Avenue");
      newAddress.setStreetNum(666);
      newAddress.setCity("Moon");
      newAddress.setState(StateType.TX);
      Phone phone = new Phone();
      phone.setExchange("6666");
      phone.setNumber("6666");
      phone.setAreaCode(66);
      newAddress.setPhoneNumber(phone);
      newBp.setAddress(newAddress);
      businessPartnerService.update(newBp);
      newBp =
         businessPartnerService.create("Rickard Oberg");
      newAddress = new Address();
      newAddress.setStreetName("Ikea Walk");
      newAddress.setStreetNum(999);
      newAddress.setCity("Stockholm");
      newAddress.setState(StateType.OH);
      phone = new Phone();
      phone.setExchange("9999");
      phone.setNumber("9999");
      phone.setAreaCode(42);
      newAddress.setPhoneNumber(phone);
      newBp.setAddress(newAddress);
      businessPartnerService.update(newBp);
      newBp =
         businessPartnerService.create("James Gosling");
      newAddress = new Address();
      newAddress.setStreetName("Sun-Set Strip");
      newAddress.setStreetNum(67362);
      newAddress.setCity("San Jose");
      newAddress.setState(StateType.IN);
      phone = new Phone();
      phone.setExchange("378");
      phone.setNumber("27874");
      phone.setAreaCode(1);
      newAddress.setPhoneNumber(phone);
      newBp.setAddress(newAddress);
      businessPartnerService.update(newBp);
      newBp =
         businessPartnerService.create("Bill \"#\" Gates");
      newAddress = new Address();
      newAddress.setStreetName(".Not Drive");
      newAddress.setStreetNum(3452);
      newAddress.setCity("Redmond");
      newAddress.setState(StateType.OH);
      phone = new Phone();
      phone.setExchange("2764");
      phone.setNumber("23782");
      phone.setAreaCode(1);
      newAddress.setPhoneNumber(phone);
      newBp.setAddress(newAddress);
      businessPartnerService.update(newBp);
   }

   /** where the config is stored */
   protected String getAxisConfiguration()
   {
      return "webservice/store/client/" + super.getAxisConfiguration();
   }

   /** test a simple hello world */
   public void testItem() throws Exception
   {
      Item newItem = itemService.create("Item that is immediately to delete");
      assertEquals("entity name", "Item that is immediately to delete", newItem.getName());
      itemService.delete(newItem);
   }

   /** test a simple hello world */
   public void testBusinessPartner() throws Exception
   {
      BusinessPartner newBp = businessPartnerService.create("Bp that is immediately to delete");
      assertEquals("entity name", "Bp that is immediately to delete", newBp.getName());
      assertNull("entity address", newBp.getAddress());
      Address address = new Address();
      StateType state = StateType.TX;
      Phone phone = new Phone();
      phone.setExchange("(0)6897");
      phone.setNumber("6666");
      phone.setAreaCode(49);
      address.setStreetNum(42);
      address.setStreetName("Milky Way");
      address.setCity("Galactic City");
      address.setZip(2121);
      address.setState(state);
      address.setPhoneNumber(phone);
      newBp.setAddress(address);
      businessPartnerService.update(newBp);

      BusinessPartner listBp = businessPartnerService.findByName("Bp that is immediately to delete");
      assertEquals("bp update name", newBp.getName(), listBp.getName());
      assertEquals("bp update address", newBp.getAddress(), listBp.getAddress());

      businessPartnerService.delete(newBp);
   }

   /** this is to deploy the whole ear */
   public static Test suite() throws Exception
   {
      return getDeploySetup(StoreUnitTestCase.class, "store.ear");
   }

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(StoreUnitTestCase.class);
   }

}