/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Phone.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store;

/**
 * A dependent class.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 21.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class Phone implements java.io.Serializable
{
   private int areaCode;
   private java.lang.String exchange;
   private java.lang.String number;

   public Phone()
   {
   }

   public int getAreaCode()
   {
      return areaCode;
   }

   public void setAreaCode(int areaCode)
   {
      this.areaCode = areaCode;
   }

   public java.lang.String getExchange()
   {
      return exchange;
   }

   public void setExchange(java.lang.String exchange)
   {
      this.exchange = exchange;
   }

   public java.lang.String getNumber()
   {
      return number;
   }

   public void setNumber(java.lang.String number)
   {
      this.number = number;
   }

   public boolean equals(Object obj)
   {
      // compare elements
      Phone other = (Phone) obj;
      if (this == obj)
         return true;
      if (!(obj instanceof Phone))
         return false;
      return areaCode == other.getAreaCode()
         && ((exchange == null && other.getExchange() == null)
         || (exchange != null && exchange.equals(other.getExchange())))
         && ((number == null && other.getNumber() == null)
         || (number != null && number.equals(other.getNumber())));
   }
}