/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.webservice.hello;

/** 
 * A serializable data object for testing data passed to an EJB through
 * the web service interface.
 * @author jung
 * @version $Revision: 1.1.2.1 $
 * @jboss-net:xml-schema urn="hello:HelloData"
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
