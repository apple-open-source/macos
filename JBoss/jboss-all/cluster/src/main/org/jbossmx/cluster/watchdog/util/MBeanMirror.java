/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

// JMX Classes
import javax.management.DynamicMBean;
import javax.management.ObjectName;

import javax.management.AttributeNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.ReflectionException;

import javax.management.Attribute;
import javax.management.AttributeList;

// Standard Java Classes
import java.rmi.RemoteException;

/**
 * @author Stacy Curl
 */
public class MBeanMirror
    implements DynamicMBean
{
    /**
     * @param    rmiAdaptor
     * @param    proxiedMBeanObjectName
     */
    public MBeanMirror(RMIAdaptor rmiAdaptor, ObjectName mirroredMBeanObjectName)
    {
        m_mirroredMBeanObjectName = mirroredMBeanObjectName;
        m_rmiAdaptor = rmiAdaptor;
    }

    /**
     * @param    attribute
     *
     * @return
     * @throws AttributeNotFoundException
     * @throws MBeanException
     * @throws ReflectionException
     */
    public Object getAttribute(String attribute)
        throws AttributeNotFoundException, MBeanException, ReflectionException
    {
        try
        {
            return getRMIAdaptor().getAttribute(getObjectName(), attribute);
        }
        catch(InstanceNotFoundException infe) {}
        catch(RemoteException re) {}

        return null;
    }

    /**
     * @param    attributes
     *
     * @return
     */
    public AttributeList getAttributes(String[] attributes)
    {
        try
        {
            return getRMIAdaptor().getAttributes(getObjectName(), attributes);
        }
        catch(InstanceNotFoundException infe) {}
        catch(RemoteException re) {}
        catch(ReflectionException re2) {}

        return null;
    }

    /**
     *
     * @return
     */
    public MBeanInfo getMBeanInfo()
    {
        try
        {
            return getRMIAdaptor().getMBeanInfo(getObjectName());
        }
        catch(InstanceNotFoundException infe) {}
        catch(IntrospectionException ie) {}
        catch(RemoteException re) {}
        catch(ReflectionException re2) {}

        return null;
    }

    /**
     * @param    actionName
     * @param    parameters
     * @param    signature
     *
     * @return
     * @throws MBeanException
     * @throws ReflectionException
     */
    public Object invoke(String actionName, Object[] parameters, String[] signature)
        throws MBeanException, ReflectionException
    {
        try
        {
            return getRMIAdaptor().invoke(getObjectName(), actionName, parameters, signature);
        }
        catch(InstanceNotFoundException infe) {}
        catch(RemoteException re) {}

        return null;
    }

    /**
     * @param    attribute
     * @throws AttributeNotFoundException
     * @throws InvalidAttributeValueException
     * @throws MBeanException
     * @throws ReflectionException
     */
    public void setAttribute(Attribute attribute)
        throws AttributeNotFoundException, InvalidAttributeValueException, MBeanException,
               ReflectionException
    {
        try
        {
            getRMIAdaptor().setAttribute(getObjectName(), attribute);
        }
        catch(InstanceNotFoundException infe) {}
        catch(RemoteException re) {}
    }

    /**
     * @param    attributes
     *
     * @return
     */
    public AttributeList setAttributes(AttributeList attributes)
    {
        try
        {
            return getRMIAdaptor().setAttributes(getObjectName(), attributes);
        }
        catch(InstanceNotFoundException infe) {}
        catch(RemoteException re) {}
        catch(ReflectionException re2) {}

        return null;
    }

    /**
     *
     * @return
     */
    private RMIAdaptor getRMIAdaptor()
    {
        return m_rmiAdaptor;
    }

    /**
     *
     * @return
     */
    private ObjectName getObjectName()
    {
        return m_mirroredMBeanObjectName;
    }

    /** */
    private RMIAdaptor m_rmiAdaptor;
    /** */
    private ObjectName m_mirroredMBeanObjectName;
}
