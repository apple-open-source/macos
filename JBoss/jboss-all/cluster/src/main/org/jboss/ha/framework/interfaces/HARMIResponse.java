/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

/** 
 *   When using HA-RMI, the result of an invocation is embedded in an instance of this class.
 *   It contains the response of the invocation and, if the list of targets has changed,
 *   a new view of the cluster.
 *
 *   @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 *   @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 *   @version $Revision: 1.4.4.1 $
 */
public class HARMIResponse 
   implements java.io.Serializable
{
   /** The serialVersionUID
    * @since 1.4
    */ 
   private static final long serialVersionUID = -2027283499335547610L;

   public java.util.ArrayList newReplicants = null;
   public long currentViewId = 0;   
   public Object response = null;
}

