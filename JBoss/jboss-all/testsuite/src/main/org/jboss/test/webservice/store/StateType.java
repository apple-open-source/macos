/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: StateType.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

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

public class StateType implements java.io.Serializable
{
   private java.lang.String _value_;
   private static java.util.HashMap _table_ = new java.util.HashMap();

   // Constructor
   protected StateType(java.lang.String value)
   {
      _value_ = value;
      _table_.put(_value_, this);
   };

   public static final java.lang.String _TX = "TX";
   public static final java.lang.String _IN = "IN";
   public static final java.lang.String _OH = "OH";
   /** @associates*/
   public static final StateType TX = new StateType(_TX);
   /** @associates*/
   public static final StateType IN = new StateType(_IN);
   /** @associates*/
   public static final StateType OH = new StateType(_OH);

   public java.lang.String getValue()
   {
      return _value_;
   }

   public static StateType fromValue(java.lang.String value)
      throws java.lang.IllegalStateException
   {
      StateType enum = (StateType) _table_.get(value);
      if (enum == null)
         throw new java.lang.IllegalStateException();
      return enum;
   }

   public static StateType fromString(String value)
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