/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins.keygenerator.uuid;

import org.jboss.ejb.plugins.keygenerator.KeyGeneratorFactory;
import org.jboss.ejb.plugins.keygenerator.KeyGenerator;

/**
 * This is the factory for UUID key generator
 *
 * @author <a href="mailto:loubyansky@ukr.net">Alex Loubyansky</a>
 *
 * @version $Revision: 1.1.2.1 $
 */
public class UUIDKeyGeneratorFactory
   implements KeyGeneratorFactory, java.io.Serializable
{
   // Constants ----------------------------------------------------

   public static final String JNDI_NAME = "UUIDKeyGeneratorFactory";

   // KeyGeneratorFactory implementation -----------------------

   /**
    * Returns the factory name
    */
   public String getFactoryName()
   {
      return JNDI_NAME;
   }

   /**
    * Returns a newly constructed key generator
    */
   public KeyGenerator getKeyGenerator()
      throws Exception
   {
      return new UUIDKeyGenerator();
   }
}
