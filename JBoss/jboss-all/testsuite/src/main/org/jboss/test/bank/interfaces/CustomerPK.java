/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.interfaces;

/**
 *      
 *   @see <related>
 *   @author $Author: oberg $
 *   @version $Revision: 1.1.1.1 $
 */
public class CustomerPK
   implements java.io.Serializable
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   public String id;
   public String name;
   
   // Public --------------------------------------------------------
   public boolean equals(Object pk)
   {
      return ((CustomerPK)pk).id.equals(id) && ((CustomerPK)pk).name.equals(name);
   }
   
   public int hashCode()
   {
      return id.hashCode();
   }
   
   public String toString()
   {
      return "Customer:"+id+"/"+name;
   }
}
