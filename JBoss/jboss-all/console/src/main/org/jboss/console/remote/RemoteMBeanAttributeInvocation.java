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
public class RemoteMBeanAttributeInvocation implements java.io.Serializable
{
   public ObjectName targetObjectName = null;
   public String attributeName = null;

   public RemoteMBeanAttributeInvocation (ObjectName pName,
                        String attribute)
   {
      this.targetObjectName = pName;
      this.attributeName = attribute;
   }

   
}
