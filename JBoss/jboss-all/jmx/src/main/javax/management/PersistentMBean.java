/*
 * LGPL
 */
package javax.management;

public interface PersistentMBean {

   public void load()
   throws MBeanException,
            RuntimeOperationsException,
            InstanceNotFoundException;

   public void store()
   throws MBeanException,
            RuntimeOperationsException,
            InstanceNotFoundException;


}

