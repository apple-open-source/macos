/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.invocation.iiop;

/**
 * Interface of a set of registries for CORBA servants. For the kinds of
 * registries it contains, see the "enum type" 
 *<code>ServantRegistryKind</code>.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public interface ServantRegistries
{
   /** 
    * Returns a <code>ServantRegistry</code> of the given <code>kind</code>.
    */
   ServantRegistry getServantRegistry(ServantRegistryKind kind);

}
