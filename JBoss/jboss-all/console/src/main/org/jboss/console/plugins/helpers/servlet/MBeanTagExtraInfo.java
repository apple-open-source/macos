/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers.servlet;

import javax.servlet.jsp.tagext.TagData;
import javax.servlet.jsp.tagext.TagExtraInfo;
import javax.servlet.jsp.tagext.VariableInfo;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>4 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class MBeanTagExtraInfo
   extends TagExtraInfo
{
   public VariableInfo[] getVariableInfo (TagData data)
   {
      return new VariableInfo[]
      {
         new VariableInfo (
               data.getAttributeString("id"),
               data.getAttributeString("intf"),
               true, 
               VariableInfo.AT_END),
      };      
   }
}
