/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.invocation.iiop;

import org.omg.PortableServer.POA;

/**
 * Interface of a CORBA reference factory. Such a factory encapsulates a POA
 * and provides reference creation methods.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public interface ReferenceFactory
{

   /** 
    * Creates a reference with a null id in its "reference data" and 
    * with object type information given by the <code>interfId</code> 
    * parameter.
    */
   org.omg.CORBA.Object createReference(String inferfId) throws Exception;

   /** 
    * Creates a reference with the specified <code>id</code> in its 
    * "reference data" and with object type information given by the
    * <code>interfId</code> parameter.
    */
   org.omg.CORBA.Object createReferenceWithId(Object id, String interfId) 
         throws Exception;

   /**
    * Returns a reference to the POA encapsulated by this 
    * <code>ReferenceFactory</code>.
    */
   POA getPOA();

}
