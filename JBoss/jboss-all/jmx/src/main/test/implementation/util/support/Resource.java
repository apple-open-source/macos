/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.util.support;

import javax.management.modelmbean.ModelMBeanInfo;
import javax.management.modelmbean.ModelMBeanInfoSupport;
import javax.management.modelmbean.ModelMBeanAttributeInfo;
import javax.management.modelmbean.ModelMBeanOperationInfo;

/**
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 */
public class Resource
{
   public ModelMBeanInfo getMBeanInfo() 
   {
      ModelMBeanAttributeInfo[] attributes = new ModelMBeanAttributeInfo[] 
      {
         new ModelMBeanAttributeInfo(
               "AttributeName", "java.lang.String", "description",
               false, true, false
         ),
         new ModelMBeanAttributeInfo(
               "AttributeName2", "java.lang.String", "description",
               true, true, false
         )
      };
      
      ModelMBeanOperationInfo[] operations = new ModelMBeanOperationInfo[]
      {
         new ModelMBeanOperationInfo(
               "doOperation", "description", null, "java.lang.Object", 1
         )
      };
      
      ModelMBeanInfoSupport info = new ModelMBeanInfoSupport(
            "test.implementation.util.support.Resource", "description",
            attributes, null, operations, null
      );
      
      return info;
   }


   public Object doOperation() 
   {
      return "tamppi";
   }

}
      



