/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee;

import java.rmi.RemoteException;
import java.util.Set;


import javax.ejb.EJBObject;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.ObjectName;
import javax.management.QueryExp;
import javax.management.ReflectionException;

/** Remote Interface of the Management EJB defined by JSR-77
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.2.4.1 $
 */
public interface Management
   extends EJBObject
{
   // Constants -----------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * Gets the value of a specific attribute of a named managed object. The
   * managed object is identified by its object name
   *
   * @param pName Object Name of the managed object from which the attribute
   *              is to be retrieved
   * @param pAttribute Name of the Attribute to be retrieved which is case
   *                   sensitive
   * @return Value of the specified attribute
   * @throws AttributeNotFound Specified attribute is not accessible in the
   *                           managed object
   * @throws ManagedObjectException Wraps an exception thrown by the manged
   *                                object's getter
   * @throws InstanceNotFoundException Specified managed object is not registered
   *                                   in the MEJB
   * @throws ReflectionException Wraps a exception thrown when trying to
   *                             the setter (??)
   * @throws RuntimeOperationsException Wraps an IllegalArgumentException
   *                                    when the given object name is null or the
   *                                    given attribute is null
   **/
   public Object getAttribute( ObjectName pName, String pAttribute )
      throws
         MBeanException,
         AttributeNotFoundException,
         InstanceNotFoundException,
         ReflectionException,
         RemoteException;
   
   /**
   * Gets the values of the specified attributes of a named managed object. The
   * managed object is identified by its object name
   *
   * @param pName Object Name of the managed object from which the attribute
   *              is to be retrieved
   * @param pAttributes Names of the Attribute to be retrieved which is case
   *                   sensitive
   * @return List of retrieved attributes
   * @throws InstanceNotFoundException Specified managed object is not registered
   *                                   in the MEJB
   * @throws ReflectionException Wraps a exception thrown when trying to
   *                             the setter (??)
   * @throws RuntimeOperationsException Wraps an IllegalArgumentException
   *                                    when the given object name is null or the
   *                                    given attribute is null
   **/
   public AttributeList getAttributes( ObjectName pName, String[] pAttributes )
      throws
         InstanceNotFoundException,
         ReflectionException,
         RemoteException;
         
   
   /**
   * @return Domain Name of this MEJB
   **/
   public String getDefaultDomain()
      throws
         RemoteException;
   
   /**
   * @returns the number of managed objects registered in the MEJB
   **/
   public Integer getMBeanCount()
      throws
         RemoteException;
   
   /**
    * Returns the MBean info about the requested Managed Object
    *
    * @param pName Object Name of the Managed Object
    *
    * @return MBeanInfo instance of the requested Managed Object
    **/
   public MBeanInfo getMBeanInfo( ObjectName pName )
      throws
         IntrospectionException,
         InstanceNotFoundException,
         ReflectionException,
         RemoteException;
   
   public ListenerRegistration getListenerRegistry()
      throws RemoteException;

   /**
   * Invokes an operation on the specified managed object
   *
   * @param pName Object Name of the managed object on which the method is to
   *              be invoked
   * @param pOperationName Name of the Operation to be invoked
   * @param pParams List of parameters to be set as parameter when the operation
   *                is invoked
   * @param pSignature List of types of the parameter list of the operation. The
   *                   class objects will be loaded using the same class loader as
   *                   the one used for loading the managed object on which the
   *                   operation was invoked
   * @return The object returned by the operation, which represents the result of
   *         invoking the operation on the managed object specified.
   * @throws InstanceNotFoundException Managed Object could not be found in the MEJB
   *                                   by specifed object name
   * @throws ManageObjectException Wraps an excepton thrown by the managed object's
   *                               invoked method
   * @throws ReflectionException Wraps an Exception thrown while trying to invoke
   *                             the method
   **/
   public Object invoke( ObjectName pName, String pOperationName, Object[] pParams, String[] pSignature )
      throws
         InstanceNotFoundException,
         MBeanException,
         ReflectionException,
         RemoteException;
   
   /**
   * Checks whether an managed object, identified by its object name, is already
   * registered with the MEJB
   *
   * @param pName Object Name of the managed object to be checked
   * @return True if the managed object is already registerred is the MEJB
   * @throws RuntimeOperationsException Wraps a IllegalArgumentException if the
   *                                    given object name is null
   **/
   public boolean isRegistered( ObjectName pName )
      throws
         RemoteException;
   
   /**
   * Gets the names of managed object controlled by the MEJB. This method enables
   * any of the following to be obtained: the names of all managed objects, the name
   * of a set of managed objects specified by pattern matching on the ObjectName,
   * a specific managed object (equivalent to testing whether an amanged object is
   * registered). When the object name is null or no domain and key properties are
   * specified, all objects are selected. It returns the set of ObjectNames for
   * the managed objects selected.
   *
   * @param pName Object Name pattern identifying the managed objects to be retrieved.
   *              If null or no domain and key properties are specified, all the
   *              managed objects registered will be retrieved.
   * @param pQuery Query Expression instance for further select the desired objecs
   * @return Set of ObjectNames for the managed objects selected. If no managed
   *         object satisfied the query, an emtpy list is returned
   **/
   public Set queryNames( ObjectName pName, QueryExp pQuery )
      throws
         RemoteException;
   
   /**
   * Sets the value of a specific attribute of a named managed object. The managed
   * object is identified by its object name
   *
   * @param pName Object Name of the managed object from which the attribute
   *              is to be retrieved
   * @param pAttribute Identification of the attribute to be set and the value it
   *                   is to be set to
   * @return Attribute with the value it was set to
   * @throws AttributeNotFoundException Specified attribute is not accessible in the
   *                                    managed object
   * @throws InstanceNotFoundException Managed Object could not be found in the MEJB
   *                                   by specifed object name
   * @throws InvalidAttributeValueException Given value for the attribute is not valid
   * @throws ReflectionException Wraps a exception thrown when trying to
   *                             the setter
   * @throws RuntimeOperationsException Wraps an IllegalArgumentException
   *                                    when the given object name is null or the
   *                                    given attribute is null
   **/
   public void setAttribute( ObjectName pName, Attribute pAttribute )
      throws
         AttributeNotFoundException,
         InstanceNotFoundException,
         InvalidAttributeValueException,
         MBeanException,
         ReflectionException,
         RemoteException;
   
   /**
   * Sets the values of the specified attributes of a named managed object. The managed
   * object is identified by its object name
   *
   * @param pName Object Name of the managed object from which the attribute
   *              is to be retrieved
   * @param pAttributes Identification of the attributes to be set and the values they
   *                    are to be set to
   * @return Attributes with the values they were set to
   * @throws InstanceNotFoundException Managed Object could not be found in the MEJB
   *                                   by specifed object name
   * @throws ReflectionException Wraps a exception thrown when trying to
   *                             the setter
   * @throws RuntimeOperationsException Wraps an IllegalArgumentException
   *                                    when the given object name is null or the given
   *                                    list of attributes is null
   **/
   public AttributeList setAttributes( ObjectName pName, AttributeList pAttributes )
      throws
         InstanceNotFoundException,
         ReflectionException,
         RemoteException;
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Static inner classes -------------------------------------------------
}
