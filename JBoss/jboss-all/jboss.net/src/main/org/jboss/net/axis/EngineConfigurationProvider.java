/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

// $Id: EngineConfigurationProvider.java,v 1.1.2.1 2002/09/12 16:18:03 cgjung Exp $
 
package org.jboss.net.axis;

import org.apache.axis.EngineConfiguration;

/**
 * Interface to any provider that may lookup the client
 * configuration with a given context name
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 9. September 2002
 * @version $Revision: 1.1.2.1 $
 */

public interface EngineConfigurationProvider 
{    
  /** returns the associated axis engine */
  public EngineConfiguration getClientEngineConfiguration(String context);
}
