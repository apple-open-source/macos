package org.jboss.test.readahead.interfaces;

import java.io.Serializable;

/**
 * Primary key class for one of the entities used in read-ahead finder tests.
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: AddressPK.java,v 1.1 2001/06/30 04:38:05 danch Exp $
 * 
 * Revision:
 */
public class AddressPK implements Serializable {

   public String key = "";
   public String addressId = "";

   public AddressPK() {
   }

   public AddressPK(String key, String addressId) {
      this.key = key;
      this.addressId = addressId;
   }
   public boolean equals(Object obj) {
      if (this.getClass().equals(obj.getClass())) {
         AddressPK that = (AddressPK) obj;
         return this.key.equals(that.key) && this.addressId.equals(that.addressId);
      }
      return false;
   }
   public int hashCode() {
      return key.hashCode()+addressId.hashCode();
   }
}