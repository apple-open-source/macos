/***************************************
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 ***************************************/

package org.jboss.console.plugins.helpers.jmx;

/** A simple tuple of an mbean operation name, sigature and result.

@author Scott.Stark@jboss.org
@version $Revision: 1.1.2.1 $
 */
public class OpResultInfo
{
   public String name;
   public String[] signature;
   public Object result;

   public OpResultInfo(String name, String[] signature, Object result)
   {
      this.name = name;
      this.signature = signature;
      this.result = result;
   }  
}
