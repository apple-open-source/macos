
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.adapter.jdbc;

import javax.resource.spi.ConnectionRequestInfo;


/**
 * WrappedConnectionRequestInfo.java
 *
 *
 * Created: Fri Apr 19 14:22:21 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class WrappedConnectionRequestInfo 
   implements ConnectionRequestInfo  
{

   private final String user;
   private final String password;

   public WrappedConnectionRequestInfo (final String user, final String password)
   {
      this.user = user;
      this.password = password;   
   }
   // implementation of javax.resource.spi.ConnectionRequestInfo interface

   /**
    *
    * @return <description>
    */
   public int hashCode()
   {
      return ((user == null)? 37: user.hashCode())
	 + 37 * ((password == null)? 37: password.hashCode());
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    */
   public boolean equals(Object other)
   {
      if (other == null || !(other.getClass() ==  WrappedConnectionRequestInfo.class))
      {
	 return false;
      }
      WrappedConnectionRequestInfo cri = (WrappedConnectionRequestInfo)other;
      if (user == null) 
      {
         if (cri.getUserName() != null) 
         {
            return false;
         } // end of if ()
      } // end of if ()
      else
      {
         if (!user.equals(cri.getUserName())) 
         {
            return false;
         } // end of if ()
      } // end of else
      if (password == null) 
      {
         if (cri.getPassword() != null) 
         {
            return false;
         } // end of if ()
      } // end of if ()
      else
      {
         if (!password.equals(cri.getPassword())) 
         {
            return false;
         } // end of if ()
      } // end of else
      return true;
   }

   String getUserName()
   {
      return user;
   }

   String getPassword()
   {
      return password;
   }

}// LocalConnectionRequestInfo
