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
public class TextInputCallback implements Callback, Serializable
{
   private String prompt;
   private String defaultText;
   private String text;

   public TextInputCallback(String prompt)
   {
      this(prompt, null);
   }
   public TextInputCallback(String prompt, String defaultText)
   {
      this.prompt = prompt;
      this.defaultText = defaultText;
   }

   public String getDefaultText()
   {
      return defaultText;
   }
   public String getPrompt()
   {
      return prompt;
   }
   public void setText(String text)
   {
      this.text = text;
   }
   public String getText()
   {
      return text;
   }
}
