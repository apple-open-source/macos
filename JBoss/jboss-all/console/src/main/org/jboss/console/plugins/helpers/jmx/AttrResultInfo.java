/***************************************
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 ***************************************/

package org.jboss.console.plugins.helpers.jmx;

import java.beans.PropertyEditor;

/** A simple tuple of an mbean operation name, sigature and result.

@author Scott.Stark@jboss.org
@version $Revision: 1.1.2.1 $
 */
public class AttrResultInfo
{
   public String name;
   public PropertyEditor editor;
   public Object result;
   public Throwable throwable;

   public AttrResultInfo(String name, PropertyEditor editor, Object result, Throwable throwable)
   {
      this.name = name;
      this.editor = editor;
      this.result = result;
      this.throwable = throwable;
   }

   public String getAsText()
   {
      if (throwable != null)
      {
         return throwable.toString();
      }
      if( result != null )
      {
         try 
         {
            if( editor != null )
            {
               editor.setValue(result);
               return editor.getAsText();
            }
            else
            {
               return result.toString();
            }
         }
         catch (Exception e)
         {
            return "String representation of " + name + "unavailable";
         } // end of try-catch
      }
      return null;
   }
}
