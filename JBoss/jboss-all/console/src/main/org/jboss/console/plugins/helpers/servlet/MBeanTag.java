/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers.servlet;

import org.jboss.console.plugins.helpers.jmx.Server;
import org.jboss.mx.util.MBeanProxy;

import javax.management.ObjectName;
import javax.servlet.jsp.JspTagException;
import javax.servlet.jsp.tagext.TagSupport;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>4 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class MBeanTag
   extends TagSupport
{
   protected String interfaceName = null;
   protected String variableName = null;
   protected String mbeanName = null;
   
   public String getIntf () { return this.interfaceName; }
   public void setIntf (String intf) { this.interfaceName = intf; }
   
   public String getId () { return this.variableName; }
   public void setId (String var) { this.variableName = var; }
   
   public String getMbean () { return this.mbeanName; }
   public void setMbean (String mbean) { this.mbeanName = mbean; }
   

   public int doStartTag () throws JspTagException
   {
      try
      {
         // Who do we proxy?
         //
         ObjectName objName = null;
         if (mbeanName == null)
         {
            objName = new ObjectName (pageContext.getRequest().getParameter("ObjectName"));
         }
         else
         {
            objName = new ObjectName (mbeanName);
         }
         
         // Which type do we proxy?
         //
         Class type = Thread.currentThread().getContextClassLoader().loadClass(this.interfaceName);
         
         // we build the proxy
         //
         Object result = MBeanProxy.get(type, objName, Server.getMBeanServer());
         
         // we assign the proxy to the variable
         //
         pageContext.setAttribute(variableName, result);
         
         return EVAL_BODY_INCLUDE;
      }
      catch (Exception e)
      {
         throw new JspTagException (e.toString());
      }
   }

   public int doEndTag () throws JspTagException
   {
      return EVAL_PAGE;
   }
}
