/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * A list of unresolved roles.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020313 Adrian Brock:</b>
 * <ul>
 * <li>Fix the cloning
 * </ul>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.4.6.1 $
 */
public class RoleUnresolvedList
  extends ArrayList
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct an empty RoleUnresolvedList.
    */
   public RoleUnresolvedList()
   {
     super();
   }

   /**
    * Construct a RoleUnresolvedList with an initial capacity.
    *
    * @param initialCapacity the initial capacity.
    */
   public RoleUnresolvedList(int initialCapacity)
   {
     super(initialCapacity);
   }

   /**
    * Construct a RoleUnresolvedList from a list. It must be an ArrayList.
    * The order of the list is maintained.
    *
    * @param list the list to copy from.
    * @exception IllegalArgumentException for a null list or
    *            an list element that is not a role unresolved.
    */
   public RoleUnresolvedList(List list)
     throws IllegalArgumentException
   {
     super();
     if (list == null)
       throw new IllegalArgumentException("Null list");
     ArrayList tmpList = new ArrayList(list);
     for (int i = 0; i < tmpList.size(); i++)
     {
       try
       {
         add((RoleUnresolved) tmpList.get(i));
       }
       catch (ClassCastException cce)
       {
         throw new IllegalArgumentException("List element is not an unresolved role.");
       }
     }
   }

   // Public ---------------------------------------------------------

   /**
    * Appends a unresolved role to the end of the list.
    * 
    * @param roleUnresolved the new unresolved role.
    * @exception IllegalArgumentException if the unresolved role is null
    */
   public void add(RoleUnresolved roleUnresolved)
     throws IllegalArgumentException
   {
     if (roleUnresolved == null)
       throw new IllegalArgumentException("Null unresolved role");
     super.add(roleUnresolved);
   }

   /**
    * Adds an unresolved role at the specified location in the list.
    * 
    * @param index the location at which to insert the unresolved role.
    * @param roleUnresolved the new unresolved role.
    * @exception IllegalArgumentException if the unresolved role is null
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list
    */
   public void add(int index, RoleUnresolved roleUnresolved)
     throws IllegalArgumentException, IndexOutOfBoundsException
   {
     if (roleUnresolved == null)
       throw new IllegalArgumentException("Null unresolved role");
     super.add(index, roleUnresolved);
   }

   /**
    * Appends an unresolved role list to the end of the list.
    * 
    * @param roleUnresolvedList the unresolved role list to append (can be null).
    * @return true if the list changes, false otherwise
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list
    */
   public boolean addAll(RoleUnresolvedList roleUnresolvedList)
     throws IndexOutOfBoundsException
   {
     if (roleUnresolvedList == null)
       return false;
     return super.addAll(roleUnresolvedList);
   }

   /**
    * Inserts an unresolved role list at the specified location in the list.
    * 
    * @param index the location at which to insert the unresolved role list.
    * @param roleUnresolvedList the unresolved role list to insert.
    * @return true if the list changes, false otherwise
    * @exception IllegalArgumentException if the unresolved role list is null
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list
    */
   public boolean addAll(int index, RoleUnresolvedList roleUnresolvedList)
     throws IllegalArgumentException, IndexOutOfBoundsException
   {
     if (roleUnresolvedList == null)
       throw new IllegalArgumentException("null roleUnresolvedList");
     return super.addAll(index, roleUnresolvedList);
   }

   /**
    * Sets an unresolved role at the specified location in the list.
    * 
    * @param index the location of the unresolved role to replace.
    * @param roleUnresolved the new unresolved role.
    * @exception IllegalArgumentException if the unresolved role is null
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list
    */
   public void set(int index, RoleUnresolved roleUnresolved)
     throws IllegalArgumentException, IndexOutOfBoundsException
   {
     if (roleUnresolved == null)
       throw new IllegalArgumentException("Null unresolved role");
     super.set(index, roleUnresolved);
   }

   // Array List Overrides -------------------------------------------

   // NONE! I think there was supposed to be?

   // Object Overrides -----------------------------------------------

   /**
    * Cloning.
    *
    * REVIEW: The spec says to return a RoleList, that's not very much
    * of a clone is it? It must be a typo in the RI.
    * 
    * @return the new unresolved role list with the same unresolved roles.
    */
   public Object clone()
   {
      return super.clone();
   }
}

