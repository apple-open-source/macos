/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.loading;

import java.net.URL;

/**
 *
 * @author  starksm
 */
class ResourceInfo
{
   URL url;
   ClassLoader cl;

   ResourceInfo(URL url, ClassLoader cl)
   {
      this.url = url;
      this.cl = cl;
   }
   
}
