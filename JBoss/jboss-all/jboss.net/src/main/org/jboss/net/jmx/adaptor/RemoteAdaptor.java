/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.net.jmx.adaptor;

import java.io.ObjectInputStream;
import java.rmi.Remote;
import java.rmi.RemoteException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.Set;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.ObjectName;
import javax.management.QueryExp;
import javax.management.ObjectInstance;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.MBeanInfo;

import javax.management.AttributeNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanRegistrationException;
import javax.management.NotCompliantMBeanException;
import javax.management.OperationsException;
import javax.management.ReflectionException;

/**
 * Remote Interface for the server side Adaptor which
 * is nearly the same as the MBeanServer Interface but
 * has an additional RemoteException.
 *
 * @author <a href="mailto:christoph.jung@jboss.org">Christoph G. Jung</a>
 **/
public interface RemoteAdaptor extends Remote
{

	// Constants -----------------------------------------------------
	
	// Public --------------------------------------------------------

	public ObjectInstance createMBean(
		String pClassName,
		ObjectName pName
	) throws
		ReflectionException,
		InstanceAlreadyExistsException,
		MBeanRegistrationException,
		MBeanException,
		NotCompliantMBeanException,
		RemoteException;

	public ObjectInstance createMBean(
		String pClassName,
		ObjectName pName,
		ObjectName pLoaderName
	) throws
		ReflectionException,
		InstanceAlreadyExistsException,
		MBeanRegistrationException,
		MBeanException,
		NotCompliantMBeanException,
		InstanceNotFoundException,
		RemoteException;

	public ObjectInstance createMBean(
		String pClassName,
		ObjectName pName,
		Object[] pParams,
		String[] pSignature
	) throws
		ReflectionException,
		InstanceAlreadyExistsException,
		MBeanRegistrationException,
		MBeanException,
		NotCompliantMBeanException,
		RemoteException;

	public ObjectInstance createMBean(
		String pClassName,
		ObjectName pName,
		ObjectName pLoaderName,
		Object[] pParams,
		String[] pSignature
	) throws
		ReflectionException,
		InstanceAlreadyExistsException,
		MBeanRegistrationException,
		MBeanException,
		NotCompliantMBeanException,
		InstanceNotFoundException,
		RemoteException;

	public void unregisterMBean(
		ObjectName pName
	) throws
		InstanceNotFoundException,
		MBeanRegistrationException,
		RemoteException;

	public ObjectInstance getObjectInstance(
		ObjectName pName
	) throws
		InstanceNotFoundException,
		RemoteException;

	public Set queryMBeans(
		ObjectName pName,
		QueryExp pQuery
	) throws
		RemoteException;

	public Set queryNames(
		ObjectName pName,
		QueryExp pQuery
	) throws
		RemoteException;

	public boolean isRegistered(
		ObjectName pName
	) throws
		RemoteException;

	public boolean isInstanceOf(
		ObjectName pName,
		String pClassName
	) throws
		InstanceNotFoundException,
		RemoteException;

	public Integer getMBeanCount(
	) throws
		RemoteException;

	public Object getAttribute(
		ObjectName pName,
		String pAttribute
	) throws
		MBeanException,
		AttributeNotFoundException,
		InstanceNotFoundException,
		ReflectionException,
		RemoteException;

	public AttributeList getAttributes(
		ObjectName pName,
		String[] pAttributes
	) throws
		InstanceNotFoundException,
		ReflectionException,
		RemoteException;

	public void setAttribute(
		ObjectName pName,
		Attribute pAttribute
	) throws
		InstanceNotFoundException,
		AttributeNotFoundException,
		InvalidAttributeValueException,
		MBeanException,
		ReflectionException,
		RemoteException;

	public AttributeList setAttributes(
		ObjectName pName,
		AttributeList pAttributes
	) throws
		InstanceNotFoundException,
		ReflectionException,
		RemoteException;

	public Object invoke(
		ObjectName pName,
		String pActionName,
		Object[] pParams,
		String[] pSignature
	) throws
		InstanceNotFoundException,
		MBeanException,
		ReflectionException,
		RemoteException;

	public String getDefaultDomain(
	) throws
		RemoteException;

	public void addNotificationListener(
		ObjectName pName,
		ObjectName pListener,
		NotificationFilter pFilter,
		Object pHandback		
	) throws
		InstanceNotFoundException,
		RemoteException;

	public void removeNotificationListener(
		ObjectName pName,
		ObjectName pListener
	) throws
		InstanceNotFoundException,
		ListenerNotFoundException,
		RemoteException;

	public MBeanInfo getMBeanInfo(
		ObjectName pName
	) throws
		InstanceNotFoundException,
		IntrospectionException,
		ReflectionException,
		RemoteException;

}
