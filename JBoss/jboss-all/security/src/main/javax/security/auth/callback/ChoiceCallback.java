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
public class ChoiceCallback implements Callback, Serializable
{
   private String prompt;
   private String[] choices;
   int defaultChoice;
   int selectedIndex;
   int[] selectedIndices;
   private boolean allowMultipleSelections;

   public ChoiceCallback(String prompt, String[] choices,  int defaultChoice,
      boolean allowMultipleSelections)
   {
      this.prompt = prompt;
      this.choices = choices;
      this.defaultChoice = defaultChoice;
      this.allowMultipleSelections = allowMultipleSelections;
   }

   public String getPrompt()
   {
      return prompt;
   }
   public String[] getChoices()
   {
      return choices;
   }
   public int getDefaultChoice()
   {
      return defaultChoice;
   }
   public boolean allowMultipleSelections()
   {
      return allowMultipleSelections;
   }
   public void setSelectedIndex(int selectedIndex)
   {
      this.selectedIndex = selectedIndex;
   }
   public void setSelectedIndexes(int[] indices)
   {
      this.selectedIndices = indices;
   }
   public int[] getSelectedIndexes()
   {
      return selectedIndices;
   }
}
