/* 
 * ====================================================================
 * This is Open Source Software, distributed
 * under the Apache Software License, Version 1.1
 * 
 * 
 *  This software  consists of voluntary contributions made  by many individuals
 *  on  behalf of the Apache Software  Foundation and was  originally created by
 *  Ivelin Ivanov <ivelin@apache.org>. For more  information on the Apache
 *  Software Foundation, please see <http://www.apache.org/>.
 */

package org.jboss.test.ha.singleton;

import java.security.InvalidParameterException;
import java.util.Stack;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.ha.singleton.HASingletonController;

/**
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HASingletonControllerTester extends HASingletonController
{

  public Stack __invokationStack__ = new Stack();

  protected Object invokeSingletonMBeanMethod(ObjectName name,
     String operationName, Object param)
     throws InstanceNotFoundException, MBeanException, ReflectionException
  {
    __invokationStack__.push("invokeMBeanMethod:" + name.getCanonicalName() + "." + operationName);
    return null;
  }

}
