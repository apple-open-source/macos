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
 * "Typesafe enum" class for the kinds of <code>ServantRegistry</code>.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class ServantRegistryKind
{
   private static final String prefixStr = "ServantRegistryKind: ";

   private final String name;

   private ServantRegistryKind(String name) { this.name = name;  }

   public static final ServantRegistryKind SHARED_TRANSIENT_POA =
      new ServantRegistryKind(prefixStr + "single transient POA");

   public static final ServantRegistryKind SHARED_PERSISTENT_POA =
      new ServantRegistryKind(prefixStr + "single persistent POA");

   public static final ServantRegistryKind TRANSIENT_POA_PER_SERVANT =
      new ServantRegistryKind(prefixStr + "transient POA per servant");

   public static final ServantRegistryKind PERSISTENT_POA_PER_SERVANT =
      new ServantRegistryKind(prefixStr + "persistent POA per servant");
}
