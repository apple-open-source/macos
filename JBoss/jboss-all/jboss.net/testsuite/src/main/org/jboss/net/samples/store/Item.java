/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Item.java,v 1.1 2002/04/02 13:48:41 cgjung Exp $

package org.jboss.net.samples.store;

import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 * Useful value-object pattern that is mapped via Axis to 
 * a corresponding remote entity bean and its management service.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1 $
 */

public interface Item {

   public String getName();
   // normally, we would hide that, but then it´s not exposed as a wsdl property
   // which is quite silly
   public void setName(String name);

	/** the client-side implementation class is just a container */
   public static class Impl implements Item {
      protected String name;

      public String getName() {
         return name;
      }

      public void setName(String name) {
         this.name = name;
      }
   }

	/** the management service for that item */
   public interface Service {
      public Item create(String name) throws StoreException;
      public void delete(Item item) throws StoreException;
      public Item[] findAll() throws StoreException;
   }

}