/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.invocation;

import java.io.Serializable;
import java.io.ObjectStreamException;


/** Type safe enumeration used for keys in the Invocation object. This relies
 * on an integer id as the identity for a key. When you add a new key enum
 * value you must assign it an ordinal value of the current MAX_KEY_ID+1 and
 * update the MAX_KEY_ID value.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public final class InvocationKey implements Serializable
{
   /** The serial version ID */
   private static final long serialVersionUID = -5117370636698417671L;

   /** The max ordinal value in use for the InvocationKey enums. When you add a
    * new key enum value you must assign it an ordinal value of the current
    * MAX_KEY_ID+1 and update the MAX_KEY_ID value.
    */
   private static final int MAX_KEY_ID = 15;

   /** The array of InvocationKey indexed by ordinal value of the key */
   private static final InvocationKey[] values = new InvocationKey[MAX_KEY_ID+1];

   /** 
    * Transactional information with the invocation. 
    */ 
   public static final InvocationKey TRANSACTION = 
         new InvocationKey("TRANSACTION", 0);

   /** 
    * Security principal assocated with this invocation.
    */
   public static final InvocationKey PRINCIPAL =
      new InvocationKey("PRINCIPAL", 1);

   /** 
    * Security credential assocated with this invocation. 
    */
   public static final InvocationKey CREDENTIAL = 
         new InvocationKey("CREDENTIAL", 2);

   /** Any authenticated Subject associated with the invocation */
   public static final InvocationKey SUBJECT = new InvocationKey("SUBJECT", 14);

   /** 
    * We can keep a reference to an abstract "container" this invocation 
    * is associated with. 
    */
   public static final InvocationKey OBJECT_NAME = 
         new InvocationKey("CONTAINER", 3);

   /** 
    * The type can be any qualifier for the invocation, anything (used in EJB). 
    */
   public static final InvocationKey TYPE = new InvocationKey("TYPE", 4);

   /** 
    * The Cache-ID associates an instance in cache somewhere on the server 
    * with this invocation. 
    */
   public static final InvocationKey CACHE_ID = new InvocationKey("CACHE_ID", 5);

   /** 
    * The invocation can be a method invocation, we give the method to call. 
    */
   public static final InvocationKey METHOD = new InvocationKey("METHOD", 6);

   /** 
    * The arguments of the method to call. 
    */
   public static final InvocationKey ARGUMENTS =
      new InvocationKey("ARGUMENTS", 7);

   /** 
    * Invocation context
    */
   public static final InvocationKey INVOCATION_CONTEXT = 
         new InvocationKey("INVOCATION_CONTEXT", 8);

   /** 
    * Enterprise context
    */
   public static final InvocationKey ENTERPRISE_CONTEXT = 
         new InvocationKey("ENTERPRISE_CONTEXT", 9);

   /** 
    * The invoker-proxy binding name
    */
   public static final InvocationKey INVOKER_PROXY_BINDING = 
         new InvocationKey("INVOKER_PROXY_BINDING", 10);

   /** 
    * The invoker 
    */
   public static final InvocationKey INVOKER = new InvocationKey("INVOKER", 11);

   /**
    * The JNDI name of the EJB.
    */
   public static final InvocationKey JNDI_NAME =
      new InvocationKey("JNDI_NAME", 12);

   /** 
    * The EJB meta-data for the {@link EJBHome} reference. 
    */
   public final static InvocationKey EJB_METADATA = 
         new InvocationKey("EJB_METADATA", 13);

   /** The EJB home proxy bound for use by getEJBHome */
   public final static InvocationKey EJB_HOME = 
         new InvocationKey("EJB_HOME", 14);

   /** The key enum symbolic value */
   private final transient String name;
   /** The persistent integer representation of the key enum */
   private final int ordinal;

   private InvocationKey(String name, int ordinal)
   {
      this.name = name;
      this.ordinal = ordinal;
      values[ordinal] = this;
   }

   public String toString()
   {
      return name;
   }

   Object readResolve() throws ObjectStreamException
   {
      return values[ordinal];
   }
}
