/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.boot.servlets;

import javax.xml.transform.TransformerException;

import org.apache.xalan.extensions.ExpressionContext;
import org.apache.xml.utils.QName;
import org.apache.xpath.objects.XObject;

import gnu.regexp.RE;
import gnu.regexp.REException;
import gnu.regexp.REMatch;

/** A utility class that implements a xalan XSLT extension function used by
the default.xsl transformation document.

 *
 * @author  Scott.Stark@jboss.org
 * @version $revision:$
 */
public class Util
{
   static RE variableRE;

   public static void setVariableRE(RE variableRE)
   {
      Util.variableRE = variableRE;
   }

   /** This function replaces all occurrences of variable references ${...} with
    the corresponding XSL variable. If no such variable is defined the variable
    is replaced with an empty string.
   */
   public static String replaceVariables(ExpressionContext ctx, String text)
   {
      String value = text;
      try
      {
         REMatch[] matches = variableRE.getAllMatches(text);
         if( matches.length > 0 )
         {
            StringBuffer tmp = new StringBuffer();
            for(int m = 0; m < matches.length; m ++)
            {
               String prefix = matches[m].toString(1);
               String name = matches[m].toString(2);
               String suffix = matches[m].toString(3);
               QName varName = new QName(name);
               XObject var = ctx.getVariableOrParam(varName);
               tmp.append(prefix);
               if( var != null )
                  tmp.append(var.toString());
               tmp.append(suffix);
            }
            value = tmp.toString();
         }
      }
      catch(TransformerException e)
      {
         e.printStackTrace();
      }
      return value;
   }

}
