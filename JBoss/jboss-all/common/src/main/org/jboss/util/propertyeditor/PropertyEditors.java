/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.beans.IntrospectionException;
import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;

import org.jboss.util.Classes;

/**
 * A collection of PropertyEditor utilities.  Provides the same interface
 * as PropertyManagerEditor plus more...
 *
 * <p>Installs the default PropertyEditors.
 *
 * @version <tt>$Revision: 1.2.2.4 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class PropertyEditors
{
   /** Augment the PropertyEditorManager search path to incorporate the JBoss
    specific editors by appending the org.jboss.util.propertyeditor package
    to the PropertyEditorManager editor search path.
    */
   static
   {
      String[] currentPath = PropertyEditorManager.getEditorSearchPath();
      int length = currentPath != null ? currentPath.length : 0;
      String[] newPath = new String[length+2];
      System.arraycopy(currentPath, 0, newPath, 0, length);
      // May want to put the JBoss editor path first, for now append it
      newPath[length] = "org.jboss.util.propertyeditor";
      newPath[length+1] = "org.jboss.mx.util.propertyeditor";
      PropertyEditorManager.setEditorSearchPath(newPath);

      /* Register the editor types that will not be found using the standard
      class name to editor name algorithm. For example, the type String[] has
      a name '[Ljava.lang.String;' which does not map to a XXXEditor name.
      */
      Class strArrayType = String[].class;
      PropertyEditorManager.registerEditor(strArrayType, StringArrayEditor.class);
   }

   /**
    * Locate a value editor for a given target type.
    *
    * @param type   The class of the object to be edited.
    * @return       An editor for the given type or null if none was found.
    */
   public static PropertyEditor findEditor(final Class type)
   {
      return PropertyEditorManager.findEditor(type);
   }

   /**
    * Locate a value editor for a given target type.
    *
    * @param typeName    The class name of the object to be edited.
    * @return            An editor for the given type or null if none was found.
    */
   public static PropertyEditor findEditor(final String typeName)
      throws ClassNotFoundException
   {
      // see if it is a primitive type first
      Class type = Classes.getPrimitiveTypeForName(typeName);
      if (type == null)
      {
         // nope try look up
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         type = loader.loadClass(typeName);
      }

      return PropertyEditorManager.findEditor(type);
   }

   /**
    * Get a value editor for a given target type.
    *
    * @param type    The class of the object to be edited.
    * @return        An editor for the given type.
    *
    * @throws RuntimeException   No editor was found.
    */
   public static PropertyEditor getEditor(final Class type)
   {
      PropertyEditor editor = findEditor(type);
      if (editor == null)
      {
         throw new RuntimeException("No property editor for type: " + type);
      }

      return editor;
   }

   /**
    * Get a value editor for a given target type.
    *
    * @param typeName    The class name of the object to be edited.
    * @return            An editor for the given type.
    *
    * @throws RuntimeException   No editor was found.
    */
   public static PropertyEditor getEditor(final String typeName)
      throws ClassNotFoundException
   {
      PropertyEditor editor = findEditor(typeName);
      if (editor == null)
      {
         throw new RuntimeException("No property editor for type: " + typeName);
      }

      return editor;
   }

   /**
    * Register an editor class to be used to editor values of a given target class.
    *
    * @param type         The class of the objetcs to be edited.
    * @param editorType   The class of the editor.
    */
   public static void registerEditor(final Class type, final Class editorType)
   {
      PropertyEditorManager.registerEditor(type, editorType);
   }

   /**
    * Register an editor class to be used to editor values of a given target class.
    *
    * @param typeName         The classname of the objetcs to be edited.
    * @param editorTypeName   The class of the editor.
    */
   public static void registerEditor(final String typeName,
                                     final String editorTypeName)
      throws ClassNotFoundException
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class type = loader.loadClass(typeName);
      Class editorType = loader.loadClass(editorTypeName);

      PropertyEditorManager.registerEditor(type, editorType);
   }

   /** Convert a string value into the true value for typeName using the
    * PropertyEditor associated with typeName.
    *
    * @param text the string represention of the value. This is passed to
    * the PropertyEditor.setAsText method.
    * @param typeName the fully qualified class name of the true value type
    * @return the PropertyEditor.getValue() result
    * @exception ClassNotFoundException thrown if the typeName class cannot
    *    be found
    * @exception IntrospectionException thrown if a PropertyEditor for typeName
    *    cannot be found
    */
   public static Object convertValue(String text, String typeName)
         throws ClassNotFoundException, IntrospectionException
   {
      // see if it is a primitive type first
      Class typeClass = Classes.getPrimitiveTypeForName(typeName);
      if (typeClass == null)
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         typeClass = loader.loadClass(typeName);
      }

      PropertyEditor editor = PropertyEditorManager.findEditor(typeClass);
      if (editor == null)
      {
         throw new IntrospectionException
               ("No property editor for type=" + typeClass);
      }

      editor.setAsText(text);
      return editor.getValue();
   }

   /**
    * Gets the package names that will be searched for property editors.
    *
    * @return   The package names that will be searched for property editors.
    */
   public String[] getEditorSearchPath()
   {
      return PropertyEditorManager.getEditorSearchPath();
   }

   /**
    * Sets the package names that will be searched for property editors.
    *
    * @param path   The serach path.
    */
   public void setEditorSearchPath(final String[] path)
   {
      PropertyEditorManager.setEditorSearchPath(path);
   }
}
