/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Unit.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store;

/**
 * An aggregated pseudo-entity.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 21.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class Unit implements java.io.Serializable
{
   private java.lang.String _value_;
   private static java.util.HashMap _table_ = new java.util.HashMap();

   // Constructor
   protected Unit(java.lang.String value)
   {
      _value_ = value;
      _table_.put(_value_, this);
   };

   public static final java.lang.String _KG = "KG";
   public static final java.lang.String _TN = "TN";
   public static final java.lang.String _STCK = "STCK";

   /** @associates */
   public static final Unit KG = new Unit(_KG);
   /** @associates */
   public static final Unit TN = new Unit(_TN);
   /** @associates */
   public static final Unit STCK = new Unit(_STCK);

   public java.lang.String getValue()
   {
      return _value_;
   }

   public static Unit fromValue(java.lang.String value)
      throws java.lang.IllegalStateException
   {
      Unit enum = (Unit)
         _table_.get(value);
      if (enum == null) throw new java.lang.IllegalStateException();
      return enum;
   }

   public static Unit fromString(String value)
      throws java.lang.IllegalStateException
   {
      return fromValue(value);
   }

   public boolean equals(Object obj)
   {
      return (obj == this);
   }

   public int hashCode()
   {
      return toString().hashCode();
   }

   public String toString()
   {
      return _value_;
   }
}
