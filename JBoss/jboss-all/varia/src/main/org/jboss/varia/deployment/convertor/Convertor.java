/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.varia.deployment.convertor;

import java.io.File;
import java.net.URL;

import org.jboss.deployment.DeploymentInfo;

/**
 * Defines the methods of a Converter
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>20020519 Andreas Schaefer:</b>
 * <ul>
 *    <li>Creation</li>
 * </ul>
 */
public interface Convertor
{
   // Public --------------------------------------------------------
   /**
    * Checks if the a deployment unit can be converted to a JBoss deployment
    * unit by this converter.
    *
    * @param url The url of the deployment unit to be converted
    *
    * @return True if this converter is able to convert
    */
   public boolean accepts(URL url);

   /**
    * Converts the necessary files to make the given deployment deployable
    * into the JBoss
    *
    * @param di Deployment info to be converted
    * @param path Path of the extracted deployment
    **/
   public void convert(DeploymentInfo di, File path)
      throws Exception;
}
