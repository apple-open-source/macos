package org.jboss.test.classloader.circularity.support;

/** A class to test protected access
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class LoginInfo
{
   protected String username = "jduke";
   protected char[] password = "theduke".toCharArray();

   public LoginInfo(String username, char[] password)
   {
      this.username = username;
      this.password = password;
   }
}
