/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins.keygenerator.uuid;

import javax.management.JMException;
import javax.management.ObjectName;
import javax.naming.Context;
import javax.naming.InitialContext;

import org.jboss.system.ServiceMBean;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.naming.Util;

import org.jboss.ejb.plugins.keygenerator.KeyGeneratorFactory;
import org.jboss.ejb.plugins.keygenerator.KeyGenerator;

/**
 * Implements UUID key generator factory service
 *
 * @jjmx:mbean name="jboss.system:service=KeyGeneratorFactory,type=UUID"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @author <a href="mailto:loubyansky@ukr.net">Alex Loubyansky</a>
 *
 * @version $Revision: 1.1.2.1 $
 */
public class UUIDKeyGeneratorFactoryService
   extends ServiceMBeanSupport
   implements UUIDKeyGeneratorFactoryServiceMBean
{

   // Attributes ----------------------------------------------------

   /** uuid key generator factory implementation */
   KeyGeneratorFactory keyGeneratorFactory;

   // ServiceMBeanSupport overridding ------------------------------

   public void startService()
   {
      // create uuid key generator factory instance
      try
      {
         keyGeneratorFactory = new UUIDKeyGeneratorFactory();
      }
      catch( Exception e ) {
         log.error( "Caught exception during startService()", e );
         // Ingore
      }

      // bind the factory
      try
      {
         Context ctx = (Context) new InitialContext();
         Util.rebind( ctx, keyGeneratorFactory.getFactoryName(),
            keyGeneratorFactory );
      }
      catch( Exception e ) {
         log.error( "Caught exception during startService()", e );
         // Ingore
      }
   }

   public void stopService()
   {
      // unbind the factory
      try
      {
         Context ctx = (Context) new InitialContext();
         Util.unbind( ctx, keyGeneratorFactory.getFactoryName() );
      }
      catch( Exception e ) {
         log.error( "Caught exception during stopService()", e );
         // Ingore
      }
   }
}
