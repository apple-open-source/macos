package org.jboss.test.classloader.circularity.support;

/** A class that uses LoginInfo to test protected access
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class UserOfLoginInfo
{
   LoginInfo info;

   public UserOfLoginInfo(String username, String password)
   {
      info = new LoginInfo(username, password.toCharArray());
   }
}
