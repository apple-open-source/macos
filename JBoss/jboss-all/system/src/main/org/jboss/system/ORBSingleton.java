/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import java.util.Properties;
import org.omg.CORBA.Any;
import org.omg.CORBA.Context;
import org.omg.CORBA.ContextList;
import org.omg.CORBA.Current;
import org.omg.CORBA.Environment;
import org.omg.CORBA.ExceptionList;
import org.omg.CORBA.INITIALIZE;
import org.omg.CORBA.NamedValue;
import org.omg.CORBA.NO_IMPLEMENT;
import org.omg.CORBA.NVList;
import org.omg.CORBA.ORBPackage.InvalidName;
import org.omg.CORBA.Request;
import org.omg.CORBA.StructMember;
import org.omg.CORBA.TCKind;
import org.omg.CORBA.TypeCode;
import org.omg.CORBA.UnionMember;
import org.omg.CORBA.ValueMember;
import org.omg.CORBA_2_3.ORB;

/**
 * Thin wrapper class that fulfills the contract of an ORB singleton and 
 * forwards every invocation to an instance of the actual ORB singleton class, 
 * which it loads with the context classloader. The name of the actual ORB 
 * singleton class is specified by the system property
 * <code>org.jboss.ORBSingletonDelegate</code>.
 * <p>
 * This class is a workaround to the the following problem: unlike the Sun VMs,
 * IBM VMs do not use the context classloader to load the ORB singleton class
 * specified by the system property 
 * <code>org.omg.CORBA.ORBSingletonClass</code>. IBM VMs use the
 * system classloader, thus requiring the ORB singleton class to be in
 * the system classpath. Rather than adding a third-party jar file (e.g. 
 * jacorb.jar) to the system classpath, we include this class in run.jar.
 * Instead of setting the system property 
 * <pre>
 *     org.omg.CORBA.ORBSingletonClass=some.orb.impl.ORBSingletonImpl
 * </pre>
 * we set two properties:
 * <pre>
 *     org.omg.CORBA.ORBSingletonClass=org.jboss.system.ORBSingleton
 *     org.jboss.ORBSingletonDelegate=some.orb.impl.ORBSingletonImpl
 * </pre>
 * <p> 
 * This class should be removed when IBM fixes its VMs.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.2 $
 */
public class ORBSingleton extends ORB
{
   /** System property key that specifies the actual ORB singleton class. */
   public static String DELEGATE_CLASS_KEY = "org.jboss.ORBSingletonDelegate";

   /** The ORB singleton instance to which all invocations are forwarded. */
   private static ORB delegate = null;
   
   /** 
    * The ORBSingleton constructor does what the IBM VM does not do: it uses
    * the context classloader to load the actual ORB singleton class.
    */
   public ORBSingleton()
   {
      if (delegate == null)
      {
         String className = System.getProperty(DELEGATE_CLASS_KEY);
         if (className == null)
            className = "org.jacorb.orb.ORBSingleton";
         try 
         {
            delegate = (ORB)Class.forName(className).newInstance();
         } 
         catch (ClassNotFoundException ex) 
         {
         }
         catch (Exception ex) {
            throw new INITIALIZE(
               "can't instantiate ORBSingleton implementation " + className);
         }

         ClassLoader cl = Thread.currentThread().getContextClassLoader();
         if (cl == null)
            cl = ClassLoader.getSystemClassLoader();
         try 
         {
            delegate = (ORB)Class.forName(className, true, cl).newInstance();
         } 
         catch (Exception ex) 
         {
            throw new INITIALIZE(
               "can't instantiate ORBSingleton implementation " + className);
         }
      }
   }

   // All the rest is pretty dumb code: it implements all the methods of the 
   // class org.omg.CORBA.ORB, either forwarding the invocation to the 
   // delegate (methods that may be called on the restricted singleton ORB
   // instance) or throwing an exception (methods that may not be called on 
   // the restricted singleton ORB instance).

   public Any create_any()
   {
      return delegate.create_any();
   } 
   
   public TypeCode create_alias_tc(String id, 
                                   String name, 
                                   TypeCode original_type)
   {
      return delegate.create_alias_tc(id, name, original_type);
   }
   
   public TypeCode create_array_tc(int length, TypeCode element_type)
   {
      return delegate.create_array_tc(length, element_type);
   }
   
   public TypeCode create_enum_tc(String id, String name, String[] members)
   {
      return delegate.create_enum_tc(id, name, members);
   }

   public TypeCode create_exception_tc(String id, 
                                       String name, 
                                       StructMember[] members)
   {
      return delegate.create_exception_tc(id, name, members);
   }
   
   public TypeCode create_interface_tc(String id, String name)
   {
      return delegate.create_interface_tc(id, name);
   }

   public TypeCode create_fixed_tc(short digits, short scale)
   {
      return delegate.create_fixed_tc(digits, scale);
   }

   public TypeCode create_recursive_tc(String id) 
   {
      return delegate.create_recursive_tc(id);
   }  
 
   /** 
    * @deprecated Deprecated by CORBA 2.3.
    */
   public TypeCode create_recursive_sequence_tc(int bound, int offset)
   {
      throw new NO_IMPLEMENT("deprecated by CORBA 2.3");
   }

   public TypeCode create_sequence_tc(int bound, TypeCode element_type)
   {
      return delegate.create_sequence_tc(bound, element_type);
   }
   
   public TypeCode create_string_tc(int bound)
   {
      return delegate.create_string_tc(bound);
   }

   public TypeCode create_wstring_tc(int bound)
   {
      return delegate.create_wstring_tc(bound);
   }

   public TypeCode create_struct_tc(String id, 
                                    String name,
                                    StructMember[] members)
   {
      return delegate.create_struct_tc(id, name, members);
   }
   
   public TypeCode create_union_tc(String id, 
                                   String name, 
                                   TypeCode discriminator_type, 
                                   UnionMember[] members)
   {
      return delegate.create_union_tc(id, name, 
                                      discriminator_type, members);
   }
   
   public TypeCode get_primitive_tc(TCKind tcKind)
   {
      return delegate.get_primitive_tc(tcKind);
   }

   public TypeCode create_value_tc(String id,
                                   String name,
                                   short type_modifier,
                                   TypeCode concrete_base,
                                   ValueMember[] members) 
    {
       return delegate.create_value_tc(id, name, type_modifier,
                                       concrete_base, members);
    }

   public TypeCode create_value_box_tc(String id, 
                                       String name, 
                                       TypeCode boxed_type) 
   {
      return delegate.create_value_box_tc(id, name, boxed_type);
   }
   
   public TypeCode create_abstract_interface_tc(String id, String name) 
   {
      return delegate.create_abstract_interface_tc(id, name);
   }
   
   public TypeCode create_native_tc(String id, String name)
   {
      return delegate.create_native_tc(id, name);
   }
   
   /* Methods not allowed on the singleton ORB: */

   public ExceptionList create_exception_list()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public NVList create_list(int count)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public NamedValue create_named_value(String name, Any value, int flags)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public NVList create_operation_list(org.omg.CORBA.Object obj)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public org.omg.CORBA.Object string_to_object(String str) 
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }

   public Environment create_environment()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
 
   public ContextList create_context_list()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public org.omg.CORBA.portable.OutputStream create_output_stream()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   /** 
    * @deprecated Deprecated by CORBA 2.3.
    */
   public Current get_current()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public Context get_default_context()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public Request get_next_response()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public String[] list_initial_services()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public String object_to_string(org.omg.CORBA.Object obj)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public  boolean poll_next_response()
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public org.omg.CORBA.Object resolve_initial_references(String identifier) 
      throws InvalidName 
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public void send_multiple_requests_deferred(Request[] req)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public  void send_multiple_requests_oneway(Request[] req)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   protected void set_parameters(String[] args, Properties props)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   protected void set_parameters(java.applet.Applet app, Properties props)
   {
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public void run() 
   {   
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public void shutdown(boolean wait_for_completion) 
   {    
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }      
   
   public boolean work_pending() 
   {     
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
   
   public void perform_work() 
   {     
      throw new NO_IMPLEMENT("The Singleton ORB only permits factory methods");
   }
}
