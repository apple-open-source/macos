/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ant;




import java.beans.PropertyEditor;
import java.beans.PropertyEditorManager;
import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import javax.management.Attribute;
import javax.management.ObjectName;
import javax.naming.Context;
import javax.naming.InitialContext;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.Task;
import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.jmx.adaptor.rmi.RMIAdaptorService;
import org.jboss.jmx.connector.RemoteMBeanServer;
import org.jboss.jmx.connector.rmi.RMIConnectorImpl;
import org.jboss.util.propertyeditor.PropertyEditors;
/**
 * JMX.java. An ant plugin to call managed operations and set attributes
 * on mbeans in a jboss jmx mbean server.
 * To use this plugin with Ant, place the jbossjmx-ant.jar together with the
 * jboss jars jboss-j2ee.jar and jboss-common-client.jar, and the sun jnet.jar in the
 * ant/lib directory you wish to use.
 *
 * Here is an example from an ant build file.
 *
 * <target name="jmx">
 *   <taskdef name="jmx"
 *	classname="org.jboss.ant.JMX"/>
 *   <jmx adapterName="jmx:HP.home.home:rmi">
 *
 *     <propertyEditor type="java.math.BigDecimal" editor="org.jboss.util.propertyeditor.BigDecimalEditor"/>
 *     <propertyEditor type="java.util.Date" editor="org.jboss.util.propertyeditor.DateEditor"/>
 *
 *
 *      <!-- define classes -->
 *     <invoke target="fgm.sysadmin:service=DefineClasses"
 *             operation="defineClasses">
 *       <parameter type="java.lang.String" arg="defineclasses.xml"/>
 *     </invoke>
 *   </jmx>
 *
 *
 * Created: Tue Jun 11 20:17:44 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:dsnyder_lion@users.sourceforge.net">David Snyder</a>
 * @version
 */

public class JMX extends Task
{


   private String serverURL;

   private String adapterName;


   private List ops = new ArrayList();

   private List editors = new ArrayList();

   /**
    * Creates a new <code>JMX</code> instance.
    * Provides a default adapterName for the current server, so you only need to set it to
    * talk to a remote server.
    *
    * @exception Exception if an error occurs
    */
   public JMX() throws Exception
   {
      adapterName = "jmx/rmi/RMIAdaptor";//org.jboss.jmx.adaptor.rmi.RMIAdaptorService.DEFAULT_JNDI_NAME;
   }

   /**
    * Use the <code>setServerURL</code> method to set the URL of the server
    * you wish to connect to.
    *
    * @param serverURL a <code>String</code> value
    */
   public void setServerURL(String serverURL)
   {
      this.serverURL = serverURL;
   }

   /**
    * Use the <code>setAdapterName</code> method to set the name the
    * adapter mbean is bound under in jndi.
    *
    * @param adapterName a <code>String</code> value
    */
   public void setAdapterName(String adapterName)
   {
      this.adapterName = adapterName;
   }

   /**
    * Use the <code>addInvoke</code> method to add an <invoke> operation.
    * Include as attributes the target ObjectName and operation name.
    * Include as sub-elements parameters: see addParameter in the Invoke class.
    *
    * @param invoke an <code>Invoke</code> value
    */
   public void addInvoke(Invoke invoke)
   {
      ops.add(invoke);
   }

   /**
    * Use the  <code>addSetAttribute</code> method to add a set-attribute
    * operation. Include as attributes the target ObjectName and the
    * the attribute name.  Include the value as a nested value tag
    * following the parameter syntax.
    *
    * @param setter a <code>Setter</code> value
    */
   public void addSetAttribute(Setter setter)
   {
      ops.add(setter);
   }

   /**
    * Use the  <code>addGetAttribute</code> method to add a get-attribute
    * operation. Include as attributes the target ObjectName, the
    * the attribute name, and a property name to hold the result of the
    * get-attribute operation.
    *
    * @param getter a <code>Getter</code> value
    */
   public void addGetAttribute(Getter getter)
   {
      ops.add(getter);
   }

   /**
    * Use the <code>addPropertyEditor</code> method to make a PropertyEditor
    * available for values.  Include attributes for the type and editor fully
    * qualified class name.
    *
    * @param peh a <code>PropertyEditorHolder</code> value
    */
   public void addPropertyEditor(PropertyEditorHolder peh)
   {
      editors.add(peh);
   }

   /**
    * The <code>execute</code> method is called by ant to execute the task.
    *
    * @exception BuildException if an error occurs
    */
   public void execute() throws BuildException
   {
      final ClassLoader origCL = Thread.currentThread().getContextClassLoader();
      try 
      {
         Thread.currentThread().setContextClassLoader(getClass().getClassLoader());
         try
         {
            for (int i = 0; i < editors.size(); i++)
            {
               ((PropertyEditorHolder)editors.get(i)).execute();
            } // end of for ()
   
   
         }
         catch (Exception e)
         {
            e.printStackTrace();
            throw new BuildException("Could not register property editors: " + e);
         } // end of try-catch
   
         try
         {
            Properties props = new Properties();
            props.put(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
            props.put(Context.URL_PKG_PREFIXES, "org.jboss.naming:org.jnp.interfaces");
   
            if (serverURL == null || "".equals(serverURL))
            {
               props.put(Context.PROVIDER_URL, "jnp://localhost:1099");
            }
            else
            {
               props.put(Context.PROVIDER_URL, serverURL);
            }
            InitialContext ctx = new InitialContext(props);;
   
            // if adapter is null, the use the default
            if (adapterName == null) {
               adapterName = "jmx/rmi/RMIAdaptor";//org.jboss.jmx.adaptor.rmi.RMIAdaptorService.DEFAULT_JNDI_NAME;
            }
   
            Object obj = ctx.lookup(adapterName);
            ctx.close();
   
            if (!(obj instanceof RMIAdaptor)) {
               throw new ClassCastException
                  ("Object not of type: RMIAdaptor, but: " +
                   (obj == null ? "not found" : obj.getClass().getName()));
            }
   
            RemoteMBeanServer server = new RMIConnectorImpl((RMIAdaptor)obj);
   
            for (int i = 0; i < ops.size(); i++)
            {
               Operation op = (Operation)ops.get(i);
               op.execute(server, this);
            } // end of for ()
   
   
         }
         catch (Exception e)
         {
            e.printStackTrace();
            throw new BuildException("problem: " + e);
         } // end of try-catch
      } 
      finally 
      {
         Thread.currentThread().setContextClassLoader(origCL);
      }

   }

   /**
    * The interface <code>Operation</code> provides a common interface
    * for the sub-tasks..
    *
    */
   public static interface Operation
   {
      void execute(RemoteMBeanServer server, Task parent) throws Exception;
   }

   /**
    * The class <code>Invoke</code> specifies the invocation of a
    * managed operation.
    *
    */
   public static class Invoke
      implements Operation
   {
      private ObjectName target;
      private String property;

      private String operation;

      private List params = new ArrayList();

      /**
       * The <code>setProperty</code> method sets the name of the property
       * that will contain the result of the operation.
       *
       * @param property a <code>String</code> value
       */
      public void setProperty(String property)
      {
          this.property = property;
      }

      /**
       * The <code>setTarget</code> method sets the ObjectName
       * of the target mbean.
       *
       * @param target an <code>ObjectName</code> value
       */
      public void setTarget(ObjectName target)
      {
         this.target = target;
      }

      /**
       * The <code>setOperation</code> method specifies the operation to
       * be performed.
       *
       * @param operation a <code>String</code> value
       */
      public void setOperation(String operation)
      {
         this.operation = operation;
      }

      /**
       * The <code>addParameter</code> method adds a parameter for
       * the operation. You must specify type and value.
       *
       * @param param a <code>Param</code> value
       */
      public void addParameter(Param param)
      {
         params.add(param);
      }

      public void execute(RemoteMBeanServer server, Task parent) throws Exception
      {
         int paramCount = params.size();
         Object[] args = new Object[paramCount];
         String[] types = new String[paramCount];
         int pos = 0;
         for (int i = 0; i < params.size(); i++)
         {
            Param p = (Param)params.get(i);
            args[pos] = p.getValue();
            types[pos] = p.getType();
            pos++;
         } // end of for ()
         Object result = server.invoke(target, operation, args, types);
         if( (property != null) && (result != null) )
         {
             parent.getProject().setProperty(property,result.toString());
         }
      }
   }

   /**
    * The class <code>Setter</code> specifies setting an attribute
    * value on an mbean.
    *
    */
   public static class Setter
      implements Operation
   {
      private ObjectName target;

      private String attribute;

      private Param value;

      /**
       * The <code>setTarget</code> method sets the ObjectName
       * of the target mbean.
       *
       * @param target an <code>ObjectName</code> value
       */
      public void setTarget(ObjectName target)
      {
         this.target = target;
      }

      /**
       * The <code>setAttribute</code> method specifies the attribute to be set.
       *
       * @param attribute a <code>String</code> value
       */
      public void setAttribute(String attribute)
      {
         this.attribute = attribute;
      }

      /**
       * The <code>setValue</code> method specifies the value to be used.
       * The type is used to convert the value to the correct type.
       *
       * @param value a <code>Param</code> value
       */
      public void setValue(Param value)
      {
         this.value = value;
      }

      public void execute(RemoteMBeanServer server, Task parent) throws Exception
      {
         Attribute att = new Attribute(attribute, value.getValue());
         server.setAttribute(target, att);
      }
   }

   /**
    * The class <code>Getter</code> specifies getting an attribute
    * value of an mbean.
    *
    */
   public static class Getter
           implements Operation
   {
       private ObjectName target;

       private String attribute;

       private String property;

       /**
        * The <code>setTarget</code> method sets the ObjectName
        * of the target mbean.
        *
        * @param target an <code>ObjectName</code> value
        */
       public void setTarget(ObjectName target)
       {
           this.target = target;
       }

       /**
        * The <code>setAttribute</code> method specifies the attribute to be
        * retrieved.
        *
        * @param attribute a <code>String</code> value
        */
       public void setAttribute(String attribute)
       {
           this.attribute = attribute;
       }

       /**
        * The <code>setProperty</code> method specifies the name of the property
        * to be set with the attribute value.
        *
        * @param property a <code>String</code> value
        */
       public void setProperty(String property)
       {
           this.property = property;
       }

       public void execute(RemoteMBeanServer server, Task parent) throws Exception
       {
           Object result = server.getAttribute(target,attribute);
           if( (property != null) && (result != null) )
           {
               parent.getProject().setProperty(property,result.toString());
           }
       }
   }

   /**
    * The class <code>Param</code> is used to represent a object by
    * means of a string representation of its value and its type.
    *
    */
   public static class Param
   {
      private String arg;
      private String type;

      /**
       * The <code>setArg</code> method sets the string representation
       * of the parameters value.
       *
       * @param arg a <code>String</code> value
       */
      public void setArg(String arg)
      {
         this.arg = arg;
      }

      public String getArg()
      {
         return arg;
      }

      /**
       * The <code>setType</code> method sets the fully qualified class
       * name of the type represented by the param object.
       *
       * @param type a <code>String</code> value
       */
      public void setType(String type)
      {
         this.type = type;
      }

      public String getType()
      {
         return type;
      }

      /**
       * The <code>getValue</code> method uses PropertyEditors to convert
       * the string representation of the value to an object, which it returns.
       * The PropertyEditor to use is determined by the type specified.
       *
       * @return an <code>Object</code> value
       * @exception Exception if an error occurs
       */
      public Object getValue() throws Exception
      {
         PropertyEditor editor = PropertyEditors.getEditor(type);
         editor.setAsText(arg);
         return editor.getValue();
      }
   }

   /**
    * The class <code>PropertyEditorHolder</code> allows you to add a
    * PropertyEditor to the default set.
    *
    */
   public static class PropertyEditorHolder
   {
      private String type;
      private String editor;

      /**
       * The <code>setType</code> method specifies the return type from the
       * property editor.
       *
       * @param type a <code>String</code> value
       */
      public void setType(final String type)
      {
         this.type = type;
      }

      public String getType()
      {
         return type;
      }

      private Class getTypeClass() throws ClassNotFoundException
      {
         //with a little luck, one of these will work with Ant's classloaders
         try
         {
            return Class.forName(type);
         }
         catch (ClassNotFoundException e)
         {
         } // end of try-catch
         try
         {
            return getClass().getClassLoader().loadClass(type);
         }
         catch (ClassNotFoundException e)
         {
         } // end of try-catch
         return Thread.currentThread().getContextClassLoader().loadClass(type);
      }

      /**
       * The <code>setEditor</code> method specifies the fully qualified
       * class name of the PropertyEditor for the type specified in the type field.
       *
       * @param editor a <code>String</code> value
       */
      public void setEditor(final String editor)
      {
         this.editor = editor;
      }

      public String getEditor()
      {
         return editor;
      }

      private Class getEditorClass() throws ClassNotFoundException
      {
         //with a little luck, one of these will work with Ant's classloaders
         try
         {
            return Class.forName(editor);
         }
         catch (ClassNotFoundException e)
         {
         } // end of try-catch
         try
         {
            return getClass().getClassLoader().loadClass(editor);
         }
         catch (ClassNotFoundException e)
         {
         } // end of try-catch
         return Thread.currentThread().getContextClassLoader().loadClass(editor);
      }

      public void execute() throws ClassNotFoundException
      {
         PropertyEditorManager.registerEditor(getTypeClass(), getEditorClass());
      }
   }
   
}// JMX
