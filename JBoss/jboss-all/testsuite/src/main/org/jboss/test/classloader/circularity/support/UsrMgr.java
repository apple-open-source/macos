package org.jboss.test.classloader.circularity.support;

/** A class to test protected access
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class UsrMgr
{
   public static void changeUserPassword(LoginInfo info, char[] password)
   {
      info.password = password;
   }
}
