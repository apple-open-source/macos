/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Information about an object registered in the MBeanServer.
 *
 * @author <a href="mailto:juha@jboss.org">Juha Lindfors</a>
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020710 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class ObjectInstance extends Object implements java.io.Serializable {

   private ObjectName name  = null;
   private String className = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID = -4099952623687795850L;
   
   public ObjectInstance(java.lang.String name,
                         java.lang.String className)
   throws MalformedObjectNameException {
      this.name = new ObjectName(name);
      this.className = className;
   
   }

   public ObjectInstance(ObjectName name,
                         java.lang.String className) {
       this.name = name;
       this.className  = className;                  
   }

   public boolean equals(java.lang.Object object) {
      if (!(object instanceof ObjectInstance)) return false;
      
      ObjectInstance oi = (ObjectInstance)object;
      return ( (name.equals(oi.getObjectName())) &&
               (className.equals(oi.getClassName())) );
   }

   public ObjectName getObjectName() {
      return name;
   }

   public java.lang.String getClassName() {
      return className;
   }


}

