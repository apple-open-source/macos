package org.jboss.test.jbossmx.implementation.server.support;

/** The standard MBean interface for ContextCL
 *
 * @author  Scott.Stark@jboss.org
 */
public interface ContextCLMBean
{
   /** An operation that retrieves a TestData object from a MarshalledObject
    *created during the ContextCL MBean ctor
    */
   public void useTestData() throws Exception;
}
