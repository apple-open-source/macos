package org.jboss.test.classloader.circularity.support;

/** A class that uses the UsrMgr class to test protected access
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class UserOfUsrMgr
{
   LoginInfo info;

   public UserOfUsrMgr(String username, String password)
   {
      info = new LoginInfo(username, password.toCharArray());
   }

   public void changePassword(char[] password)
   {
      UsrMgr.changeUserPassword(info, password);
   }
}
