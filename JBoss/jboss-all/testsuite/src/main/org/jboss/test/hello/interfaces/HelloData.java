/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.hello.interfaces;

/** A serializable data object for testing data passed to an EJB.
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.3 $
 */
public class HelloData
   implements java.io.Serializable
{
   private String name;

   public String getName()
   {
      return name;
   }
   public void setName(String name)
   {
      this.name = name;
   }
}
