/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ResetClassLoaderHandler.java,v 1.2 2002/04/26 11:53:25 cgjung Exp $

package org.jboss.net.axis;

import org.apache.axis.AxisFault;
import org.apache.axis.MessageContext;
import org.apache.axis.handlers.BasicHandler;

/**
 * This handler is to restore a previously changed classpath and
 * should be put in most cases into a response chain.
 * <br>
 * <h3>Change notes</h3>
 *   <ul>
 *   </ul>
 * @created  11.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.2 $
 */

public class ResetClassLoaderHandler extends BasicHandler {

   //
   // Protected Helpers
   //

	/** reset the thread associated classloader to some old value stored in the 
	 *  message context
	 */
   protected void resetClassLoader(MessageContext msgContext) {
      ClassLoader loader = (ClassLoader) msgContext.getProperty(Constants.OLD_CLASSLOADER_PROPERTY);
      if (loader != null) {
         msgContext.setProperty(Constants.OLD_CLASSLOADER_PROPERTY, null);
         Thread.currentThread().setContextClassLoader(loader);
      }
   }

   //
   // API
   //

   /*
    * @see Handler#invoke(MessageContext)
    */
   public void invoke(MessageContext msgContext) {
      resetClassLoader(msgContext);
   }

   /*
    * @see Handler#onFault(MessageContext)
    */
   public void onFault(MessageContext msgContext) {
      resetClassLoader(msgContext);
   }

}