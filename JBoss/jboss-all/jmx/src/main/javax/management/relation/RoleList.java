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
 * A list of roles.<p>
 *
 * I think the idea is supposed to be that only roles should be in the
 * list. But this isn't true.
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
public class RoleList
  extends ArrayList
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct an empty RoleList.
    */
   public RoleList()
   {
     super();
   }

   /**
    * Construct a RoleList with an initial capacity.
    *
    * @param initialCapacity the initial capacity.
    */
   public RoleList(int initialCapacity)
   {
     super(initialCapacity);
   }

   /**
    * Construct a RoleList from a list. It must be an ArrayList.
    * The order of the list is maintained.
    *
    * @param list the list to copy from.
    * @exception IllegalArgumentException for a null list or
    *            an list element that is not a role.
    */
   public RoleList(List list)
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
         add((Role) tmpList.get(i));
       }
       catch (ClassCastException cce)
       {
         throw new IllegalArgumentException("List element is not a role.");
       }
     }
   }

   // Public ---------------------------------------------------------

   /**
    * Appends a role to the end of the list.
    * 
    * @param role the new role.
    * @exception IllegalArgumentException if the role is null
    */
   public void add(Role role)
     throws IllegalArgumentException
   {
     if (role == null)
       throw new IllegalArgumentException("Null role");
     super.add(role);
   }

   /**
    * Adds a role at the specified location in the list.
    * 
    * @param index the location at which to insert the role.
    * @param role the new role.
    * @exception IllegalArgumentException if the role is null
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list
    */
   public void add(int index, Role role)
     throws IllegalArgumentException, IndexOutOfBoundsException
   {
     if (role == null)
       throw new IllegalArgumentException("Null role");
     super.add(index, role);
   }

   /**
    * Appends a role list to the end of the list.
    * 
    * @param roleList the role list to insert, can be null
    * @return true if the list changes, false otherwise
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list (this is part of ArrayList for some reason?)
    */
   public boolean addAll(RoleList roleList)
     throws IndexOutOfBoundsException
   {
     if (roleList == null)
       return false;
     return super.addAll(roleList);
   }

   /**
    * Inserts a role list at the specified location in the list.
    * 
    * @param index the location at which to insert the role list.
    * @param roleList the role list to insert.
    * @return true if the list changes, false otherwise
    * @exception IllegalArgumentException if the role list is null
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list
    */
   public boolean addAll(int index, RoleList roleList)
     throws IllegalArgumentException, IndexOutOfBoundsException
   {
     if (roleList == null)
       throw new IllegalArgumentException("null roleList");
     return super.addAll(index, roleList);
   }

   /**
    * Sets a role at the specified location in the list.
    * 
    * @param index the location of the role to replace.
    * @param role the new role.
    * @exception IllegalArgumentException if the role is null
    * @exception IndexOutOfBoundsException if there is no such index
    *            in the list
    */
   public void set(int index, Role role)
     throws IllegalArgumentException, IndexOutOfBoundsException
   {
     if (role == null)
       throw new IllegalArgumentException("Null role");
     super.set(index, role);
   }

   // Array List Overrides -------------------------------------------

   // NONE! I think there was supposed to be?

   // Object Overrides -----------------------------------------------

   /**
    * Cloning, creates a new RoleList with the same elements.
    * The roles in the list are not cloned.
    * 
    * @return the new empty role list.
    */
   public Object clone()
   {
       return super.clone();
   }
}

