/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.interceptor;

import javax.management.Descriptor;

/**
 * Abstraction of the invocation that travels through the interceptor
 * stack.
 *
 * @see org.jboss.mx.interceptor.Interceptor
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3.8.1 $
 *
 */
public class Invocation
{
   // Constants -----------------------------------------------------
   public final static int READ  = 0x1;
   public final static int WRITE = 0x2;

   public final static int OPERATION = 0x10;
   public final static int ATTRIBUTE = 0x20;

   // Attributes ----------------------------------------------------
   private Object sx;
   private Object tx;
   private int type;
   private int impact;
   private Object[] args;
   private String name;
   private String[] signature;
   private String fullName;
   private Descriptor[] descriptors;
   private Object resource;

   // Constructors --------------------------------------------------
   public Invocation(String name, int type, int impact, Object[] args,
      String[] signature, Descriptor[] descriptors, Object resource)
   {
      this.name = name;
      this.type = type;
      this.impact = impact;
      this.args = args;
      this.signature = signature;
      this.descriptors = descriptors;
      this.resource = resource;
   }

   // Public --------------------------------------------------------
   public Object[] getArgs()
   {
      return args;
   }

   public Descriptor[] getDescriptors()
   {
      return descriptors;
   }

   public int getInvocationType()
   {
      return type;
   }

   public int getImpact()
   {
      return impact;
   }

   public String getName()
   {
      return name;
   }

   public Object getResource()
   {
      return resource;
   }

   public String[] getSignature()
   {
      return signature;
   }

   public String getOperationWithSignature()
   {
      if (signature == null && type == OPERATION)
         return name;

      if (fullName != null)
         return fullName;

      StringBuffer strBuf = new StringBuffer(1000);

      if (type == ATTRIBUTE && impact == READ)
         strBuf.append("get");
      if (type == ATTRIBUTE && impact == WRITE)
         strBuf.append("set");

      strBuf.append(name);
      if (signature != null)
         for (int i = 0; i < signature.length; ++i)
            strBuf.append(signature[i]);

      fullName = strBuf.toString();
      return fullName;
   }
}
