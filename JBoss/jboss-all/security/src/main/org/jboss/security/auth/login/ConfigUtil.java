package org.jboss.security.auth.login;

import java.util.ArrayList;
import java.util.HashMap;

import javax.security.auth.login.AppConfigurationEntry;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.Node;

/** Utility methods for parsing the XMlLoginConfig elements into
 * AuthenticationInfo instances.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class ConfigUtil
{
   /** Parse the application-policy/authentication element
    @param policy , the application-policy/authentication element
    @return the AuthenticationInfo object for the xml policy fragment
    */
   static public AuthenticationInfo parseAuthentication(Element policy)
      throws Exception
   {
      // Parse the permissions
      NodeList authentication = policy.getElementsByTagName("authentication");
      if (authentication.getLength() == 0)
      {
         return null;
      }

      Element auth = (Element) authentication.item(0);
      NodeList modules = auth.getElementsByTagName("login-module");
      ArrayList tmp = new ArrayList();
      for (int n = 0; n < modules.getLength(); n++)
      {
         Element module = (Element) modules.item(n);
         parseModule(module, tmp);
      }
      AppConfigurationEntry[] entries = new AppConfigurationEntry[tmp.size()];
      tmp.toArray(entries);
      AuthenticationInfo info = new AuthenticationInfo();
      info.setAppConfigurationEntry(entries);
      return info;
   }

   static void parseModule(Element module, ArrayList entries)
      throws Exception
   {
      AppConfigurationEntry.LoginModuleControlFlag controlFlag = AppConfigurationEntry.LoginModuleControlFlag.REQUIRED;
      String className = module.getAttribute("code");
      String flag = module.getAttribute("flag");
      if (flag != null)
      {
         // Lower case is what is used by the jdk1.4.1 implementation
         flag = flag.toLowerCase();
         if (AppConfigurationEntry.LoginModuleControlFlag.REQUIRED.toString().indexOf(flag) > 0)
            controlFlag = AppConfigurationEntry.LoginModuleControlFlag.REQUIRED;
         else if (AppConfigurationEntry.LoginModuleControlFlag.REQUISITE.toString().indexOf(flag) > 0)
            controlFlag = AppConfigurationEntry.LoginModuleControlFlag.REQUISITE;
         else if (AppConfigurationEntry.LoginModuleControlFlag.SUFFICIENT.toString().indexOf(flag) > 0)
            controlFlag = AppConfigurationEntry.LoginModuleControlFlag.SUFFICIENT;
         else if (AppConfigurationEntry.LoginModuleControlFlag.OPTIONAL.toString().indexOf(flag) > 0)
            controlFlag = AppConfigurationEntry.LoginModuleControlFlag.OPTIONAL;
      }
      NodeList opts = module.getElementsByTagName("module-option");
      HashMap options = new HashMap();
      for (int n = 0; n < opts.getLength(); n++)
      {
         Element opt = (Element) opts.item(n);
         String name = opt.getAttribute("name");
         String value = getContent(opt, "");
         options.put(name, value);
      }
      AppConfigurationEntry entry = new AppConfigurationEntry(className, controlFlag, options);
      entries.add(entry);
   }

   static String getContent(Element element, String defaultContent)
   {
      NodeList children = element.getChildNodes();
      String content = defaultContent;
      if (children.getLength() > 0)
      {
         content = "";
         for (int n = 0; n < children.getLength(); n++)
         {
            if (children.item(n).getNodeType() == Node.TEXT_NODE ||
               children.item(n).getNodeType() == Node.CDATA_SECTION_NODE)
               content += children.item(n).getNodeValue();
            else
               content += children.item(n).getFirstChild();
         }
         return content.trim();
      }
      return content;
   }
}
