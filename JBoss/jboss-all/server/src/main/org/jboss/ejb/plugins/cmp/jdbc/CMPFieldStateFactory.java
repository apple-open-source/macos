/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

/**
 * Implementations of this interface are used to create and compare field states
 * for equality.
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public interface CMPFieldStateFactory
{
   /**
    * Calculates and returns an object that represents the state of the field value.
    * The states produced by this method will be used to check whether the field
    * is dirty at synchronization time.
    *
    * @param fieldValue  field's value.
    * @return an object representing the field's state.
    */
   Object getFieldState(Object fieldValue);

   /**
    * Checks whether the field's state <code>state</code>
    * is equal to the field value's state (possibly, calculated with
    * the <code>getFieldState()</code> method).
    *
    * @param state  the state to compare with field value's state.
    * @param fieldValue  field's value, the state of which will be compared
    *                    with <code>state</code>.
    * @return  true if <code>state</code> equals to <code>fieldValue</code>'s state.
    */
   boolean isStateValid(Object state, Object fieldValue);
}
