/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.varia.property;

import java.beans.PropertyEditorManager;
import java.beans.PropertyEditor;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.StringTokenizer;
import java.util.List;
import java.util.LinkedList;
import java.util.Iterator;
import java.util.Properties;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import org.jboss.system.ServiceMBeanSupport;

/**
 * A service to access <tt>java.beans.PropertyEditorManager</tt>.
 *
 * @jmx:mbean name="jboss.varia:type=Service,name=PropertyEditorManager"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class PropertyEditorManagerService
   extends ServiceMBeanSupport
   implements PropertyEditorManagerServiceMBean
{
   ///////////////////////////////////////////////////////////////////////////
   //                      PropertyEditorManager Access                     //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * Locate a value editor for a given target type.
    *
    * @jmx:managed-operation
    *
    * @param type   The class of the object to be edited.
    * @return       An editor for the given type or null if none was found.
    */
   public PropertyEditor findEditor(final Class type)
   {
      return PropertyEditorManager.findEditor(type);
   }

   /**
    * Locate a value editor for a given target type.
    *
    * @jmx:managed-operation
    *
    * @param typeName    The class name of the object to be edited.
    * @return            An editor for the given type or null if none was found.
    */
   public PropertyEditor findEditor(final String typeName)
      throws ClassNotFoundException
   {
      Class type = Class.forName(typeName);
      
      return PropertyEditorManager.findEditor(type);
   }

   /**
    * Register an editor class to be used to editor values of a given target class.
    *
    * @jmx:managed-operation
    *
    * @param type         The class of the objetcs to be edited.
    * @param editorType   The class of the editor.
    */
   public void registerEditor(final Class type, final Class editorType)
   {
      PropertyEditorManager.registerEditor(type, editorType);
   }

   /**
    * Register an editor class to be used to editor values of a given target class.
    *
    * @jmx:managed-operation
    *
    * @param typeName         The classname of the objetcs to be edited.
    * @param editorTypeName   The class of the editor.
    */
   public void registerEditor(final String typeName,
                              final String editorTypeName)
      throws ClassNotFoundException
   {
      Class type = Class.forName(typeName);
      Class editorType = Class.forName(editorTypeName);

      PropertyEditorManager.registerEditor(type, editorType);
   }

   /** Turn a string[] into an comma seperated list. */
   private String makeString(final String[] array)
   {
      StringBuffer buff = new StringBuffer();
      
      for (int i=0; i<array.length; i++) {
         buff.append(array[i]);
         if ((i + 1) < array.length) {
            buff.append(",");
         }
      }

      return buff.toString();
   }

   /** Turn an comma seperated list into a string[]. */
   private String[] makeArray(final String listspec)
   { 
      StringTokenizer stok = new StringTokenizer(listspec, ",");
      List list = new LinkedList();
      
      while (stok.hasMoreTokens()) {
         String url = stok.nextToken();
         list.add(url);
      }

      return (String[])list.toArray(new String[list.size()]);
   }

   /**
    * Gets the package names that will be searched for property editors.
    *
    * @jmx:managed-attribute
    *
    * @return   The package names that will be searched for property editors.
    */
   public String getEditorSearchPath()
   {
      return makeString(PropertyEditorManager.getEditorSearchPath());
   }

   /**
    * Sets the package names that will be searched for property editors.
    *
    * @jmx:managed-attribute
    *
    * @param path   A comma sperated list of package names.
    */
   public void setEditorSearchPath(final String path)
   {
      PropertyEditorManager.setEditorSearchPath(makeArray(path));
   }
   

   ///////////////////////////////////////////////////////////////////////////
   //                      JMX & Configuration Helpers                      //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * Load property editors based on the given properties string.
    *
    * @jmx:managed-attribute
    *
    * @param props, A string representation of a editor.class=editor.type
    * Properties map for the editors to load.
    */
   public void setBootstrapEditors(final String propsString)
      throws ClassNotFoundException, IOException
   {
      Properties props = new Properties();
      ByteArrayInputStream stream = new ByteArrayInputStream(propsString.getBytes());
      props.load(stream);
      setEditors(props);
   }

   /**
    * Set property editors based on the given properties map.
    *
    * @jmx:managed-attribute
    *
    * @param props    Map of <em>type name</em> to </em>editor type name</em>.
    */
   public void setEditors(final Properties props) throws ClassNotFoundException
   {
      Iterator iter = props.keySet().iterator();
      while (iter.hasNext()) {
         String typeName = (String)iter.next();
         String editorTypeName = props.getProperty(typeName);

         registerEditor(typeName, editorTypeName);
      }
   }

   
   ///////////////////////////////////////////////////////////////////////////
   //                     ServiceMBeanSupport Overrides                     //
   ///////////////////////////////////////////////////////////////////////////

   protected ObjectName getObjectName(final MBeanServer server, final ObjectName name)
      throws MalformedObjectNameException
   {
      return name == null ? OBJECT_NAME : name;
   }
   
}
