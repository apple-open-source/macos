/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import java.lang.reflect.Method;

import javax.management.InstanceNotFoundException;
import javax.management.AttributeNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import javax.management.RuntimeOperationsException;
import javax.management.RuntimeMBeanException;
import javax.management.RuntimeErrorException;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 */
public interface ProxyExceptionHandler
{
   public Object handleInstanceNotFound(ProxyContext ctx, InstanceNotFoundException e, Method m, Object[] args) throws Exception;
   
   public Object handleAttributeNotFound(ProxyContext ctx, AttributeNotFoundException e, Method m, Object[] args) throws Exception;
   
   public Object handleInvalidAttributeValue(ProxyContext ctx, InvalidAttributeValueException e, Method m, Object[] args) throws Exception;
   
   public Object handleMBeanException(ProxyContext ctx, MBeanException e, Method m, Object[] args) throws Exception;
   
   public Object handleReflectionException(ProxyContext ctx, ReflectionException e, Method m, Object[] args) throws Exception;
   
   public Object handleRuntimeOperationsException(ProxyContext ctx, RuntimeOperationsException e, Method m, Object[] args) throws Exception;
   
   public Object handleRuntimeMBeanException(ProxyContext ctx, RuntimeMBeanException e, Method m, Object[] args) throws Exception;
   
   public Object handleRuntimeError(ProxyContext ctx, RuntimeErrorException e, Method m, Object[] args) throws Exception;
}
      



