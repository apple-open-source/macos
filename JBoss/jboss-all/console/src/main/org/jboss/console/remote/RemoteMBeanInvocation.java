/*
 * RemoteMBeanInvocation.java
 *
 * Created on 21. avril 2003, 19:25
 */

package org.jboss.console.remote;

import javax.management.ObjectName;

/**
 * Stupid JMX invocation implementation. Should support attributes as well, etc.
 * 
 * @author  Sacha Labourey
 */
public class RemoteMBeanInvocation implements java.io.Serializable
{
   public ObjectName targetObjectName = null;
   public String actionName = null;
   public Object[] params = null;
   public String[] signature = null;

   public RemoteMBeanInvocation (ObjectName pName,
                        String pActionName,
                        Object[] pParams,
                        String[] pSignature) 
   {
      this.targetObjectName = pName;
      this.actionName = pActionName;
      this.params = pParams;
      this.signature = pSignature;
   }   

   
}
