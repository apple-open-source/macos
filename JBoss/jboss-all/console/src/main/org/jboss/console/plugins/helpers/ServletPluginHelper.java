/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>23 dec 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class ServletPluginHelper
   extends javax.servlet.http.HttpServlet
{
   
   // Constants -----------------------------------------------------
   
   public static final String WRAPPER_CLASS_PARAM = "WrapperClass";
   
   // Attributes ----------------------------------------------------
   
   protected ServletConfig config = null;
   
   protected PluginWrapper wrapper = null;      

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
      
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // HttpServlet overrides -----------------------------------------
   
   public void init (ServletConfig config) throws ServletException
   {
      try
      {
         super.init (config);      
         
         this.config = config;
         
         wrapper = createPluginWrapper ();      
         wrapper.init (config);      
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new ServletException (e);
      }
       
   }


   public void destroy ()
   {
      super.destroy ();      
      
      wrapper.destroy ();
   }
 
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected PluginWrapper createPluginWrapper () throws Exception
   {
      String tmp = config.getInitParameter(WRAPPER_CLASS_PARAM);
      if (tmp != null && !"".equals(tmp))
      {
         // These plugins do provide their own wrapper implementation
         //
         Class clazz = Thread.currentThread().getContextClassLoader().loadClass(tmp);
         return (PluginWrapper) (clazz.newInstance());
      }
      
      
      // Otherwise we make the hypothesis that the script provides
      // all required information
      //
      return new BasePluginWrapper ();
      
   }
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

}
