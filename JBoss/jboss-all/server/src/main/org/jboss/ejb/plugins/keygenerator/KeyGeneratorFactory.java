/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins.keygenerator;

/**
 * This is the factory for key generators.
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 *
 * @version $Revision: 1.1.2.1 $
 */
public interface KeyGeneratorFactory
{
   /**
    * Returns the name of the factory
    */
   public String getFactoryName();

   /**
    * Returns a new key generator
    */
   public KeyGenerator getKeyGenerator()
      throws Exception;
}
