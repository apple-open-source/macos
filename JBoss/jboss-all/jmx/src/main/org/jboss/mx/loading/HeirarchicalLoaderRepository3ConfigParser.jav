package org.jboss.mx.loading;

import java.io.ByteArrayInputStream;
import java.util.Properties;

import org.jboss.mx.loading.LoaderRepositoryFactory.LoaderRepositoryConfigParser;

/** The LoaderRepositoryConfigParser implementation for the HeirarchicalLoaderRepository3.
 * This implementation supports the single java2ParentDelegation property which
 * indicates whether the HeirarchicalLoaderRepository3 should load classes from
 * its scope first followed by its parent repository (java2ParentDelegation=true).
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class HeirarchicalLoaderRepository3ConfigParser
   implements LoaderRepositoryConfigParser
{
   /** Set the HeirarchicalLoaderRepository3.UseParentFirst attribute based on
    * the value of the java2ParentDelegation property found in the config.
    *
    * @param repository the HeirarchicalLoaderRepository3 to set the
    * UseParentFirst attribute on.
    * @param config A string representation of a Properties file
    * @throws Exception
    */
   public void configure(LoaderRepository repository, String config)
      throws Exception
   {
      HeirarchicalLoaderRepository3 hlr3 = (HeirarchicalLoaderRepository3) repository;
      Properties props = new Properties();
      ByteArrayInputStream bais = new ByteArrayInputStream(config.getBytes());
      props.load(bais);
      String java2ParentDelegation = props.getProperty("java2ParentDelegation");
      if( java2ParentDelegation == null )
      {
         // Check for previous mis-spelled property name
         java2ParentDelegation = props.getProperty("java2ParentDelegaton", "false");
      }
      boolean useParentFirst = Boolean.valueOf(java2ParentDelegation).booleanValue();
      hlr3.setUseParentFirst(useParentFirst);
   }
}
