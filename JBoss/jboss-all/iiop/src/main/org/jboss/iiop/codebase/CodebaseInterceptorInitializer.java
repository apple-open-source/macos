/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.iiop.codebase;

import org.omg.CORBA.LocalObject;
import org.omg.IOP.Codec;
import org.omg.IOP.Encoding;
import org.omg.IOP.ENCODING_CDR_ENCAPS;
import org.omg.PortableInterceptor.ORBInitializer;
import org.omg.PortableInterceptor.ORBInitInfo;

/**
 * Implements an <code>org.omg.PortableInterceptor.ORBinitializer</code> that
 * installs a <code>CodebaseInterceptor</code>, a 
 * <code>CodebasePolicyFactory</code> and a <code>ServerInterceptor</code>.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.2 $
 */
public class CodebaseInterceptorInitializer 
      extends LocalObject
      implements ORBInitializer
{
   
   public CodebaseInterceptorInitializer() 
   {
      // do nothing
   }

   // org.omg.PortableInterceptor.ORBInitializer operations -------------------

   public void pre_init(ORBInitInfo info) 
   {
      // do nothing
   }
   
   public void post_init(ORBInitInfo info) 
   {
      try {
         // Use CDR encapsulation with GIOP 1.0 encoding
         Encoding encoding = new Encoding(ENCODING_CDR_ENCAPS.value, 
                                          (byte)1, /* GIOP version */
                                          (byte)0  /* GIOP revision*/);
         Codec codec = info.codec_factory().create_codec(encoding);
         info.add_ior_interceptor(new CodebaseInterceptor(codec));
         info.register_policy_factory(CodebasePolicy.TYPE, 
                                      new CodebasePolicyFactory());
      }
      catch (Exception e) {
         throw new RuntimeException("Unexpected " + e);
      }
   }
   
}
