/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth.callback;

import java.io.Serializable;

/** The JAAS 1.0 classes for use of the JAAS authentication classes with
 * JDK 1.3. Use JDK 1.4+ to use the JAAS authorization classes provided by
 * the version of JAAS bundled with JDK 1.4+.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class NameCallback implements Callback, Serializable
{
   private String prompt;
   private String defaultName;
   private String name;

   public NameCallback(String prompt)
   {
      this(prompt, null);
   }
   public NameCallback(String prompt, String defaultName)
   {
      this.prompt = prompt;
      this.defaultName = defaultName;
   }

   public String getDefaultName()
   {
      return defaultName;
   }

   public String getName()
   {
      return name;
   }
   public void setName(String name)
   {
      this.name = name;
   }

   public String getPrompt()
   {
      return prompt;
   }
}
