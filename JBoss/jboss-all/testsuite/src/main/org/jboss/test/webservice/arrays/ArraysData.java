/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.webservice.arrays;

/** 
 * A serializable data object for testing data passed to an EJB through
 * the web service interface.
 * @author jung
 * @version $Revision: 1.1.2.1 $
 * @jboss-net:xml-schema urn="arrays:ArraysData"
 */

public class ArraysData
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

   public boolean equals(Object obj)
   {
      if (this == obj) return true;
      if (obj == null || (obj instanceof ArraysData) == false) return false;
      ArraysData other = (ArraysData) obj;
      if (name == null && other.name == null) return true;
      if (name == null && other.name != null) return false;
      return (name.equals(other.name));
   }

   public int hashCode()
   {
      if (name == null)
         return 0;
      else
         return name.hashCode();
   }
}
