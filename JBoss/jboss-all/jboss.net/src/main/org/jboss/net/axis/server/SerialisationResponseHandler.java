/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: SerialisationResponseHandler.java,v 1.1.4.1 2002/09/12 16:18:04 cgjung Exp $

package org.jboss.net.axis.server;

import org.apache.axis.AxisFault;
import org.apache.axis.MessageContext;
import org.apache.axis.handlers.BasicHandler;

/**
 * This handler is to force serialisation inside transaction and
 * security boundaries.
 * <br>
 * <h3>Change notes</h3>
 *   <ul>
 *   </ul>
 * @created  22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.4.1 $
 */

public class SerialisationResponseHandler extends BasicHandler {
   
   //
   // API
   //

   /**
    * force deserialisation by accessing the msgcontext.
    * @see Handler#invoke(MessageContext)
    */
   public void invoke(MessageContext msgContext) throws AxisFault {
      msgContext.getResponseMessage().getContentLength();
   }

}