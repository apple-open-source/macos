/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import java.util.Collection;
import java.util.ConcurrentModificationException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.HashSet;
import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.LocalProxyFactory;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;

/**
 * This is the relationship set.  An instance of this class
 * is returned when collection valued cmr field is accessed.
 * See the EJB 2.0 specification for a more detailed description
 * or the responsibilities of this class.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.9.2.7 $
 */
public class RelationSet implements Set
{
   private JDBCCMRFieldBridge cmrField;
   private EntityEnterpriseContext ctx;
   private List[] setHandle;
   private boolean readOnly;
   private Class relatedLocalInterface;

   //
   // Most of this class is a boring wrapper arround the id set.
   // The only interesting hitch is the setHandle.  This class doesn't
   // have a direct referance to the related id set, it has a referance
   // to a referance to the set. When the transaction is completed the
   // CMR field sets my referance to the set to null, so that I know that
   // this set is no longer valid. See the ejb spec for more info.
   //
   public RelationSet(
      JDBCCMRFieldBridge cmrField,
      EntityEnterpriseContext ctx,
      List[] setHandle,
      boolean readOnly)
   {

      this.cmrField = cmrField;
      this.ctx = ctx;
      this.setHandle = setHandle;
      this.readOnly = readOnly;
      relatedLocalInterface = cmrField.getRelatedLocalInterface();
   }

   private List getIdList()
   {
      if(setHandle[0] == null)
      {
         throw new IllegalStateException("A CMR collection may only be used " +
            "within the transction in which it was created");
      }
      return setHandle[0];
   }

   public int size()
   {
      List idList = getIdList();
      return idList.size();
   }

   public boolean isEmpty()
   {
      List idList = getIdList();
      return idList.isEmpty();
   }

   public boolean add(Object o)
   {
      if(o == null)
      {
         throw new IllegalArgumentException("Null cannot be added to a CMR " +
            "relationship collection");
      }

      checkForPKChange();

      List idList = getIdList();
      if(readOnly)
      {
         throw new EJBException("This collection is a read-only snapshot");
      }

      if(cmrField.isReadOnly())
      {
         throw new EJBException("Field is read-only: " +
            cmrField.getFieldName());
      }

      if(!relatedLocalInterface.isInstance(o))
      {
         String msg = "Object must be an instance of " +
            relatedLocalInterface.getName() + ", but is an isntance of [";
         Class[] classes = o.getClass().getInterfaces();
         for(int i = 0; i < classes.length; i++)
         {
            if(i > 0) msg += ", ";
            msg += classes[i].getName();
         }
         msg += "]";
         throw new IllegalArgumentException(msg);
      }

      Object id = ((EJBLocalObject) o).getPrimaryKey();
      if(idList.contains(id))
      {
         return false;
      }
      cmrField.createRelationLinks(ctx, id);
      return true;
   }

   public boolean addAll(Collection c)
   {
      if(readOnly)
      {
         throw new EJBException("This collection is a read-only snapshot");
      }
      if(cmrField.isReadOnly())
      {
         throw new EJBException("Field is read-only: " +
            cmrField.getFieldName());
      }

      if(c == null)
      {
         return false;
      }

      boolean isModified = false;

      Iterator iterator = (new HashSet(c)).iterator();
      while(iterator.hasNext())
      {
         isModified = add(iterator.next()) || isModified;
      }
      return isModified;
   }

   public boolean remove(Object o)
   {
      List idList = getIdList();
      if(readOnly)
      {
         throw new EJBException("This collection is a read-only snapshot");
      }
      if(cmrField.isReadOnly())
      {
         throw new EJBException("Field is read-only: " +
            cmrField.getFieldName());
      }

      checkForPKChange();

      if(!relatedLocalInterface.isInstance(o))
      {
         throw new IllegalArgumentException("Object must be an instance of " +
            relatedLocalInterface.getName());
      }

      Object id = ((EJBLocalObject) o).getPrimaryKey();
      if(!idList.contains(id))
      {
         return false;
      }
      cmrField.destroyRelationLinks(ctx, id);
      return true;
   }

   public boolean removeAll(Collection c)
   {
      if(readOnly)
      {
         throw new EJBException("This collection is a read-only snapshot");
      }
      if(cmrField.isReadOnly())
      {
         throw new EJBException("Field is read-only: " +
            cmrField.getFieldName());
      }

      if(c == null)
      {
         return false;
      }

      boolean isModified = false;

      Iterator iterator = (new HashSet(c)).iterator();
      while(iterator.hasNext())
      {
         isModified = remove(iterator.next()) || isModified;
      }
      return isModified;
   }

   public void clear()
   {
      checkForPKChange();

      List idList = getIdList();
      if(readOnly)
      {
         throw new EJBException("This collection is a read-only snapshot");
      }
      if(cmrField.isReadOnly())
      {
         throw new EJBException("Field is read-only: " +
            cmrField.getFieldName());
      }

      Iterator iterator = (new ArrayList(idList)).iterator();
      while(iterator.hasNext())
      {
         cmrField.destroyRelationLinks(ctx, iterator.next());
      }
   }

   public boolean retainAll(Collection c)
   {
      List idList = getIdList();
      if(readOnly)
      {
         throw new EJBException("This collection is a read-only snapshot");
      }
      if(cmrField.isReadOnly())
      {
         throw new EJBException("Field is read-only: " +
            cmrField.getFieldName());
      }

      checkForPKChange();

      if(c == null)
      {
         if(idList.size() == 0)
         {
            return false;
         }
         clear();
         return true;
      }

      // get a set of the argument collection's ids
      List argIds = new ArrayList();
      Iterator iterator = c.iterator();
      while(iterator.hasNext())
      {
         argIds.add(((EJBLocalObject) iterator.next()).getPrimaryKey());
      }

      boolean isModified = false;

      iterator = (new ArrayList(idList)).iterator();
      while(iterator.hasNext())
      {
         Object id = iterator.next();
         if(!argIds.contains(id))
         {
            cmrField.destroyRelationLinks(ctx, id);
            isModified = true;
         }
      }
      return isModified;
   }

   public boolean contains(Object o)
   {
      List idList = getIdList();

      if(!relatedLocalInterface.isInstance(o))
      {
         throw new IllegalArgumentException("Object must be an instance of " +
            relatedLocalInterface.getName());
      }

      Object id = ((EJBLocalObject) o).getPrimaryKey();
      return idList.contains(id);
   }

   public boolean containsAll(Collection c)
   {
      List idList = getIdList();

      if(c == null)
      {
         return true;
      }

      // get a set of the argument collection's ids
      List argIds = new ArrayList();
      Iterator iterator = c.iterator();
      while(iterator.hasNext())
      {
         argIds.add(((EJBLocalObject) iterator.next()).getPrimaryKey());
      }

      return idList.containsAll(argIds);
   }

   public Object[] toArray(Object a[])
   {
      List idList = getIdList();

      Collection c = cmrField.getRelatedInvoker().getEntityLocalCollection(idList);
      return c.toArray(a);
   }

   public Object[] toArray()
   {
      List idList = getIdList();
      Collection c = cmrField.getRelatedInvoker().getEntityLocalCollection(idList);
      return c.toArray();
   }

   // Private

   private static void checkForPKChange()
   {
      /**
       * Uncomment to disallow attempts to override PK value with equal FK value
       *
       if(cmrField.getRelatedCMRField().allFkFieldsMappedToPkFields()) {
       throw new IllegalStateException(
       "Can't modify relationship: CMR field "
       + cmrField.getRelatedEntity().getEntityName() + "." + cmrField.getRelatedCMRField().getFieldName()
       + " has _ALL_ foreign key fields mapped to the primary key columns."
       + " Primary key may only be set once in ejbCreate [EJB 2.0 Spec. 10.3.5].");
       }
       */
   }

   // Inner

   public Iterator iterator()
   {
      return new Iterator()
      {
         private final Iterator idIterator = getIdList().iterator();
         private final LocalProxyFactory localFactory = cmrField.getRelatedInvoker();
         private Object currentId;

         public boolean hasNext()
         {
            verifyIteratorIsValid();

            try
            {
               return idIterator.hasNext();
            }
            catch(ConcurrentModificationException e)
            {
               throw new IllegalStateException("Underlying collection has " +
                  "been modified");
            }
         }

         public Object next()
         {
            verifyIteratorIsValid();

            try
            {
               currentId = idIterator.next();
               return localFactory.getEntityEJBLocalObject(currentId);
            }
            catch(ConcurrentModificationException e)
            {
               throw new IllegalStateException("Underlying collection has " +
                  "been modified");
            }
         }

         public void remove()
         {
            verifyIteratorIsValid();
            if(readOnly)
            {
               throw new EJBException("This collection is a read-only snapshot");
            }
            if(cmrField.isReadOnly())
            {
               throw new EJBException("Field is read-only: " +
                  cmrField.getFieldName());
            }

            checkForPKChange();

            try
            {
               idIterator.remove();
               cmrField.destroyRelationLinks(ctx, currentId, false);
            }
            catch(ConcurrentModificationException e)
            {
               throw new IllegalStateException("Underlying collection has been modified");
            }
         }

         private void verifyIteratorIsValid()
         {
            if(setHandle[0] == null)
            {
               throw new IllegalStateException("The iterator of a CMR " +
                  "collection may only be used within the transction in " +
                  "which it was created");
            }
         }
      };
   }

   public String toString()
   {
      return new StringBuffer()
         .append('[')
         .append(cmrField.getEntity().getEntityName())
         .append('.')
         .append(cmrField.getFieldName())
         .append(':')
         .append(getIdList()).append(']')
         .toString();
   }
}
