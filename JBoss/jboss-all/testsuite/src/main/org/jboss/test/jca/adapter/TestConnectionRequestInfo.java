
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.adapter; // Generated package name


import javax.resource.spi.ConnectionRequestInfo;

/**
 * TestConnectionRequestInfo.java
 *
 *
 * Created: Mon Dec 31 17:14:13 2001
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestConnectionRequestInfo implements ConnectionRequestInfo
{
   public String failure = "nowhere";

   public TestConnectionRequestInfo ()
   {

   }

   public TestConnectionRequestInfo (String failure)
   {
      this.failure = failure;
   }

   // implementation of javax.resource.spi.ConnectionRequestInfo interface

   /**
    *
    * @return <description>
    */
   public int hashCode()
   {
     return failure.hashCode();
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    */
   public boolean equals(Object param1)
   {
     if (param1 == this) return true;
     if (param1 == null || (param1 instanceof TestConnectionRequestInfo) == false) return false;
     TestConnectionRequestInfo other = (TestConnectionRequestInfo) param1;
     return failure.equals(other.failure);
   }


}
