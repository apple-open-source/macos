/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.server;

import java.io.File;
import java.io.Serializable;
import java.net.MalformedURLException;

import javax.management.ObjectName;

import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.deployment.scanner.URLDeploymentScannerMBean;

/** 
 *
 * @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.7.4.5 $
 *
 * <p><b>20020809 bill burke:</b>
 * <ul>
 *   <li>Initial import
 * </ul>
 */
public interface FarmMemberServiceMBean 
   extends URLDeploymentScannerMBean
{
   /** The default object name. */
   ObjectName OBJECT_NAME = ObjectNameFactory.create("jboss:service=FarmMember");
   
   /**
   * Name of the Partition used as a cluster definition for the farming
   **/
   public String getPartitionName ();
   public void setPartitionName (String partitionName);

   /** Backward compatibility, mapped to the URLs attribute of URLDeploymentScannerMBean
    * @deprecated
    */
   public void setFarmDeployDirectory(String urls)
      throws MalformedURLException;
   /** Backward compatibility, but ignored as it does nothing.
    * @deprecated
    */
   public void setScannerName(String name);

   public static class FileContent implements Serializable 
   {
      public File mFile;
      public byte[] mContent;
      
      public FileContent( File pFile, byte[] pContent ) 
      {
         mFile = pFile;
         mContent = pContent;
      }
   }
}
