/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Address.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store;

import java.io.Serializable;

/**
 * A dependent class.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class Address implements Serializable
{
   protected int streetNum;
   protected String streetName;
   protected String city;
   /**
    * @link aggregation
    * @clientCardinality *
    * @supplierCardinality 0..1
    * @label liesIn
    */
   protected StateType state;
   protected int zip;
   /**
    * @link aggregation
    * @label reachedUnder
    * @clientCardinality *
    * @supplierCardinality 0..1
    */
   protected Phone phoneNumber;

   public int getStreetNum()
   {
      return streetNum;
   }

   public void setStreetNum(int streetNum)
   {
      this.streetNum = streetNum;
   }

   public java.lang.String getStreetName()
   {
      return streetName;
   }

   public void setStreetName(java.lang.String streetName)
   {
      this.streetName = streetName;
   }

   public java.lang.String getCity()
   {
      return city;
   }

   public void setCity(java.lang.String city)
   {
      this.city = city;
   }

   public StateType getState()
   {
      return state;
   }

   public void setState(StateType state)
   {
      this.state = state;
   }

   public int getZip()
   {
      return zip;
   }

   public void setZip(int zip)
   {
      this.zip = zip;
   }

   public Phone getPhoneNumber()
   {
      return phoneNumber;
   }

   public void setPhoneNumber(Phone phoneNumber)
   {
      this.phoneNumber = phoneNumber;
   }

   public boolean equals(Object obj)
   {
      if (this == obj) return true;
      if (!(obj instanceof Address)) return false;
      // compare elements
      Address other = (Address) obj;

      return
         streetNum == other.getStreetNum() &&
         ((streetName == null && other.getStreetName() == null) ||
         (streetName != null &&
         streetName.equals(other.getStreetName()))) &&
         ((city == null && other.getCity() == null) ||
         (city != null &&
         city.equals(other.getCity()))) &&
         ((state == null && other.getState() == null) ||
         (state != null &&
         state.equals(other.getState()))) &&
         zip == other.getZip() &&
         ((phoneNumber == null && other.getPhoneNumber() == null) ||
         (phoneNumber != null &&
         phoneNumber.equals(other.getPhoneNumber())));
   }
}