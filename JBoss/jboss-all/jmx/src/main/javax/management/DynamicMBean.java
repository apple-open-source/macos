/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;


/**
 * The DynamicMBean interface is implemented by resources that expose 
 * their definition at runtime. The implementation exposes its attributes, 
 * methods and notifications through the {@link #getMBeanInfo() getMBeanInfo} 
 * method.<p>
 * 
 * From a management point of view, the DynamicMBean behaves like any other
 * MBean.<p>
 *
 * It is the responsibility of the implementation to ensure the MBean works
 * as self-described. The MBeanServer makes no attempt to validate it. <p>
 * 
 * Although the self-description is retrieved at runtime and is therefore
 * dynamic, the implementation must not change it once it has been retrieved. 
 * A manager can expect the attributes, methods and notifications to 
 * remain constant. It is the implementation's responsibility to conform to 
 * this behaviour.
 *
 * @see javax.management.MBeanInfo
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 *   
 */
public interface DynamicMBean
{

   // Public ------------------------------------------------------   

   /**
    * Returns the value of the attribute with the name matching the
    * passed string.
    *
    * @param attribute the name of the attribute.
    * @return the value of the attribute.
    * @exception AttributeNotFoundException when there is no such attribute.
    * @exception MBeanException wraps any error thrown by the resource when 
    * getting the attribute.
    * @exception ReflectionException wraps any error invoking the resource.
    */
   public Object getAttribute(String attribute)
      throws AttributeNotFoundException, MBeanException, ReflectionException;

   /**
    * Sets the value of an attribute. The attribute and new value are
    * passed in the name value pair {@link Attribute Attribute}.
    *
    * @see javax.management.Attribute
    *
    * @param attribute the name and new value of the attribute.
    * @exception AttributeNotFoundException when there is no such attribute.
    * @exception InvalidAttributeValueException when the new value cannot be
    * converted to the type of the attribute.
    * @exception MBeanException wraps any error thrown by the resource when
    * setting the new value.
    * @exception ReflectionException wraps any error invoking the resource.
    */
   public void setAttribute(Attribute attribute)
      throws AttributeNotFoundException, InvalidAttributeValueException,
             MBeanException, ReflectionException;


   /**
    * Returns the values of the attributes with names matching the
    * passed string array.
    *
    * @param attributes the names of the attribute.
    * @return an {@link AttributeList AttributeList} of name and value pairs.
    */
   public AttributeList getAttributes(java.lang.String[] attributes);

   /**
    * Sets the values of the attributes passed as an 
    * {@link AttributeList AttributeList} of name and new value pairs.
    *
    * @param attributes the name an new value pairs.
    * @return an {@link AttributeList AttributeList} of name and value pairs 
    * that were actually set.
    */
   public AttributeList setAttributes(AttributeList attributes);

   /**
    * Invokes a resource operation. 
    *
    * @param actionName the name of the operation to perform.
    * @param params the parameters to pass to the operation.
    * @param signature the signartures of the parameters.
    * @return the result of the operation.
    * @exception MBeanException wraps any error thrown by the resource when
    * performing the operation.
    * @exception ReflectionException wraps any error invoking the resource.
    */
   public java.lang.Object invoke(java.lang.String actionName,
                                  java.lang.Object[] params,
                                  java.lang.String[] signature)
      throws MBeanException, ReflectionException;

   /**
    * Returns the management interface that describes this dynamic resource.
    * It is the responsibility of the implementation to make sure the
    * description is accurate.
    *
    * @return the management interface.
    */
   public MBeanInfo getMBeanInfo();
}
