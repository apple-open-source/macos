/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.resource.ResourceException;
import javax.resource.spi.ManagedConnectionMetaData;

/**
 * ???
 *
 * Created: Sat Mar 31 03:36:27 2001
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version $Revision: 1.1 $
 */
public class JmsMetaData
   implements ManagedConnectionMetaData
{
   private JmsManagedConnection mc;
   
   public JmsMetaData(final JmsManagedConnection mc) {
      this.mc = mc;
   }
   
   public String getEISProductName() throws ResourceException {
      return "JMS CA Resource Adapter";
   }

   public String getEISProductVersion() throws ResourceException {
      return "0.1";//Is this possible to get another way
   }

   public int getMaxConnections() throws ResourceException {
      // Dont know how to get this, from Jms, we
      // set it to unlimited
      return 0;
   }
    
   public String getUserName() throws ResourceException {
      return mc.getUserName();
   }
}
