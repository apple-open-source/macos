/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

/** 
 *
 *   @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 *   @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 *   @version $Revision: 1.3.4.1 $
 */
public interface HARMIProxy 
   extends java.io.Serializable
{
   /** The serialVersionUID
    * @since 1.3
    */ 
   static final long serialVersionUID = 3106067731192293020L;

   public boolean isLocal();
}