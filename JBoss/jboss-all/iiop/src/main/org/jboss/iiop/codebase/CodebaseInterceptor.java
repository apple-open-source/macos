/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.iiop.codebase;

import org.omg.CORBA.Any;
import org.omg.CORBA.LocalObject;
import org.omg.CORBA.ORB;
import org.omg.IOP.Codec;
import org.omg.IOP.CodecPackage.InvalidTypeForEncoding;
import org.omg.IOP.TAG_JAVA_CODEBASE;
import org.omg.IOP.TaggedComponent;
import org.omg.PortableInterceptor.IORInfo;
import org.omg.PortableInterceptor.IORInterceptor;

/**
 * Implements an <code>org.omg.PortableInterceptor.IORInterceptor</code>
 * that adds a Java codebase component to an IOR.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class CodebaseInterceptor
      extends LocalObject
      implements IORInterceptor
{
   private Codec codec;

   public CodebaseInterceptor(Codec codec)
   {
      this.codec = codec;
   }

   // org.omg.PortableInterceptor.IORInterceptor operations -------------------

   public String name()
   {
      return CodebaseInterceptor.class.getName();
   }

   public void destroy()
   {
   }

   public void establish_components(IORInfo info) 
   {
      // Get CodebasePolicy object
      CodebasePolicy codebasePolicy= 
         (CodebasePolicy)info.get_effective_policy(CodebasePolicy.TYPE);

      if (codebasePolicy != null) {
         // Get codebase string from CodebasePolicy
         String codebase = codebasePolicy.getCodebase();
         
         // Encapsulate codebase string into TaggedComponent
         Any any = ORB.init().create_any();
         any.insert_string(codebase);
         byte[] taggedComponentData;
         try {
            taggedComponentData = codec.encode_value(any);
         }
         catch (InvalidTypeForEncoding e) {
            throw new RuntimeException("Exception establishing " +
                                       "Java codebase component:" + e);
         }
         info.add_ior_component(new TaggedComponent(TAG_JAVA_CODEBASE.value, 
                                                    taggedComponentData));
      }
   }
   
}
