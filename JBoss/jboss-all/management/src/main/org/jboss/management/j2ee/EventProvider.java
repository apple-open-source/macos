package org.jboss.management.j2ee;

/**
 * Indicates the emitting of Events and also
 * indicates what types of events it emits.
 * <br><br>
 * <b>Attention</b>: This interface is not indented to be used by the client
 * but it is morely a desription of what the client will get when he/she
 * receives attributes from the JSR-77 server or what method can be
 * invoked.
 * <br><br>
 * All attributes (getXXX) can be received by
 * <br>
 * {@link javax.management.j2ee.Management#getAttribute Management.getAttribute()}
 * <br>
 * or in bulk by:
 * <br>
 * {@link javax.management.j2ee.Management#getAttributes Management.getAttributes()}
 * <br>
 * Methods (all except getXXX()) can be invoked by:
 * <br>
 * {@link javax.management.j2ee.Management#invoke Management.invoke()}
 * <br>
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.2.2.2 $
 */
public interface EventProvider
{
   
   /**
   * @return The actual list of Types of Events this Managed Object emits.
   *         The list is never null nor empty
   */
   public String[] getEventTypes();
   
   /**
   * Returns the given Type of Events it emits according to its index in the list
   *
   * @param index Index of the requested Event Type
   *
   * @return Event Type if given Index is within the boundaries otherwise null
   */
   public String getEventType( int index );
}
