/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

/**
 * Indicates that this Managed Object supports all the
 * operations and attributes to support state management.
 * An Managed Object implementing this interface is
 * termed as State Manageable Object (SMO).
 * An SMO generates events when its state changes.
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
 * @version $Revision: 1.4.2.1 $
 */
public interface StateManageable
   extends EventProvider
{
   // Constants -----------------------------------------------------
   
   public static final int STARTING = 0;
   public static final int RUNNING = 1;
   public static final int STOPPING = 2;
   public static final int STOPPED = 3;
   public static final int FAILED = 4;
   
   // Public --------------------------------------------------------
   
   /**
   * @return The Time (in milliseconds since 1/1/1970 00:00:00) that this
   *         managed object was started
   */
   public long getStartTime();

   /**
   * @return Current State of the SMO which could be either {@link #STARTING
   *         starting}, {@link #RUNNING running}, {@link #STOPPING stopping},
   *         {@link #STOPPED stopped} or {@link FAILED failed}
   */
   public int getState();

   /**
   * @return Current State string from amont {@link #STARTING
   *         STARTING}, {@link #RUNNING RUNNING}, {@link #STOPPING STOPPING},
   *         {@link #STOPPED STOPPED} or {@link FAILED FAILED}
   */
   public String getStateString();

   /**
   * Starts this SMO which can only be invoked when the SMO is in the State
   * {@link #STOPPED stopped}. The SMO will go into the State {@link @STARTING
   * started} and after it completes successfully the SMO will go to the State
   * {@link #RUNNING running}.
   * The children of the SMO will not be started by this method call.
   *
   * <b>Attention</b>: According to the specification this is named <i>start()</i>
   *                   but in order to avoid name conflicts this is renamed to
   *                   <i>mejbStart()</i>. The MEJB interface will make the conversion
   *                   from <i>start</i> to <i>mejbStart</i> to make it transparent
   *                   to the client.
   */
   public void mejbStart();

   /**
   * Starts this SMO like {@link @start start()}. After the SMO is started all
   * its children in the State of {@link @STOPPED stopped} theirs startRecursive()
   * are started too.
   *
   * <b>Attention</b>: According to the specification this is named <i>startRecursive()</i>
   *                   but in order to avoid name conflicts this is renamed to
   *                   <i>mejbStartRecursive()</i>. The MEJB interface will make the conversion
   *                   from <i>startRecursive</i> to <i>mejbStartRecursive</i> to make it transparent
   *                   to the client.
   */
   public void mejbStartRecursive();

   /**
   * Stops this SMO which can only be into the {@link #RUNNING running} or
   * {@link #STARTING starting}. The State will change to {@link #STOPPING
   * stoping} and after it completes successfully it will go into the
   * State {@link #STOPPED stopped}. Afterwards all its children stop()
   * method is called.
   *
   * <b>Attention</b>: According to the specification this is named <i>stop()</i>
   *                   but in order to avoid name conflicts this is renamed to
   *                   <i>mejbStop()</i>. The MEJB interface will make the conversion
   *                   from <i>stop</i> to <i>mejbStop</i> to make it transparent
   *                   to the client.
   */
   public void mejbStop();

}
