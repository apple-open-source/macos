package org.jboss.test.jmx.xmbean;

import java.beans.PropertyEditorSupport;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class CustomTypeEditor extends PropertyEditorSupport
{
   /** Convert "x.y" text to a CustomType(x, y)
    *
    * @param text
    * @throws IllegalArgumentException
    */
   public void setAsText(String text) throws IllegalArgumentException
   {
      int dot = text.indexOf('.');
      if( dot < 0 )
         throw new IllegalArgumentException("CustomType text must be 'x.y'");
      int x = Integer.parseInt(text.substring(0, dot));
      int y = Integer.parseInt(text.substring(dot+1));
      setValue(new CustomType(x, y));
   }

   public String getAsText()
   {
      CustomType type = (CustomType) getValue();
      return type.getX() + "." + type.getY();
   }
}
