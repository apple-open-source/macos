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
public class ConfirmationCallback implements Callback, Serializable
{
   public static final int UNSPECIFIED_OPTION = -1;
   public static final int YES_NO_OPTION = 0;
   public static final int YES_NO_CANCEL_OPTION = 1;
   public static final int OK_CANCEL_OPTION = 2;
   public static final int YES = 0;
   public static final int NO = 1;
   public static final int CANCEL = 2;
   public static final int OK = 3;
   public static final int INFORMATION = 0;
   public static final int WARNING = 1;
   public static final int ERROR = 2;

   private String prompt;
   private String[] options;
   int defaultOption;
   int optionType = UNSPECIFIED_OPTION;
   int messageType;
   int selectedIndex;
   int[] selectedIndices;

   public ConfirmationCallback(int messageType, int optionType,
      int defaultOption)
   {
      this(null, messageType, optionType, defaultOption);
   }
   public ConfirmationCallback(int messageType, String[] options,
      int defaultOption)
   {
      this.messageType = messageType;
      this.options = options;
      this.defaultOption = defaultOption;
   }
   public ConfirmationCallback(String prompt, int messageType,
      int optionType, int defaultOption)
   {
      this.prompt = prompt;
      this.messageType = messageType;
      this.optionType = optionType;
      this.defaultOption = defaultOption;
   }
   public ConfirmationCallback(String prompt, int messageType,
      String[] options, int defaultOption)
   {
      this.prompt = prompt;
      this.messageType = messageType;
      this.options = options;
      this.defaultOption = defaultOption;
   }

   public String getPrompt()
   {
      return prompt;
   }
   public int getMessageType()
   {
      return messageType;
   }
   public int getOptionType()
   {
      return optionType;
   }
   public String[] getOptions()
   {
      return options;
   }
   public int getDefaultOption()
   {
      return defaultOption;
   }
   public void setSelectedIndex(int index)
   {
      this.selectedIndex = index;
   }
   public int getSelectedIndex()
   {
      return selectedIndex;
   }
}
