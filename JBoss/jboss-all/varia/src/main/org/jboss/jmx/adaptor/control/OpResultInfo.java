/***************************************
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 ***************************************/

package org.jboss.jmx.adaptor.control;

/**
 * A simple tuple of an mbean operation name,
 * index, sigature, args and operation result.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.1 $
 */
public class OpResultInfo
{
   public String   name;
   public String[] signature;
   public String[] args;
   public Object   result;

   public OpResultInfo(String name, String[] signature, String[] args, Object result)
   {
      this.name      = name;
      this.signature = signature;
      this.args      = args;
      this.result    = result;
   }
}
