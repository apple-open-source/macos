/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.metadata;

import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;
import java.io.InputStream;
import org.jboss.mx.service.ServiceConstants;

/**
 * XMBeanEntityResolver.java
 *
 *
 * Created: Sun Sep  1 20:54:10 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class XMBeanEntityResolver implements EntityResolver, ServiceConstants
{
   public XMBeanEntityResolver()
   {

   }

   public InputSource resolveEntity(String publicId, String systemId)
   {
      try
      {
         if (publicId.equals(PUBLIC_JBOSSMX_XMBEAN_DTD_1_0))
         {
            InputStream dtdStream =
               getClass().getResourceAsStream(
                  "/metadata/" + JBOSSMX_XMBEAN_DTD_1_0);

            return new InputSource(dtdStream);

         } // end of if ()
         if (publicId.equals(PUBLIC_JBOSSMX_XMBEAN_DTD_1_1))
         {
            InputStream dtdStream =
               getClass().getResourceAsStream(
                  "/metadata/" + JBOSSMX_XMBEAN_DTD_1_1);

            return new InputSource(dtdStream);

         } // end of if ()

         /*this one doesn't exist in source, so I'm leaving it out.
         if (publicId.endsWith(XMBEAN_DTD)) 
         {
            InputStream dtdStream = getClass().getResourceAsStream(XMBEAN_DTD);
            return new InputSource(dtdStream);
         
         } // end of if ()
         */
      }
      catch (Exception ignore)
      {
      }
      return null;
   }

} // XMBeanEntityResolver
