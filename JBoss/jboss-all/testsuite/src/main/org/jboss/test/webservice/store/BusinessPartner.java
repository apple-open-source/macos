/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: BusinessPartner.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store;

/**
 * Value-object pattern that is mapped via Axis to 
 * a corresponding remote entity bean.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public interface BusinessPartner
{

   public String getName();

   public void setName(String name);

   public Address getAddress();

   public void setAddress(Address address);

   /** client-side implementation */
   public static class Impl implements BusinessPartner
   {
      protected String name;
      protected Address address;

      public String getName()
      {
         return name;
      }

      public void setName(String name)
      {
         this.name = name;
      }

      public Address getAddress()
      {
         return address;
      }

      public void setAddress(Address address)
      {
         this.address = address;
      }
   }

   /** interface to a management service */
   public interface Service
   {
      public BusinessPartner create(String name) throws StoreException;

      public void delete(BusinessPartner bp) throws StoreException;

      public BusinessPartner[] findAll() throws StoreException;

      public void update(BusinessPartner bp);

      public BusinessPartner findByName(String name) throws StoreException;
   }

}