/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.metadata;

import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;

/**
 * AttributeOperationResolver is a modified TST for mapping an Integer code against attribute and operation keys.
 *
 * Note that this implementation was chosen to allow fast resolution of compound keys - namely the
 * operationName and signature[] passed to an MBean's invoke() method.  For consistency it also
 * keeps track of attribute names (which are a single key), although for those a hashmap would
 * have done just as well.
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */
public class AttributeOperationResolver
{
   private Node opRoot = null;
   private Node atRoot = null;

   /**
    * Default constructor.
    */
   public AttributeOperationResolver()
   {
   }

   /**
    * Uses the AttributeInfo and OperationInfo arrays in the MBeanInfo to configure the
    * resolver.  Each attribute and operation will be assigned a code which corresponds
    * to it's position in the info array.
    */
   public AttributeOperationResolver(MBeanInfo info)
   {
      this(info.getAttributes(), info.getOperations());
   }

   /**
    * Uses the AttributeInfo and OperationInfo arrays to configure the resolver.
    * Each attribute and operation will be assigned a code which corresponds to it's
    * position in the info array.
    */
   public AttributeOperationResolver(MBeanAttributeInfo[] attributes, MBeanOperationInfo[] operations)
   {
      int attributeCount = (attributes != null) ? attributes.length : 0;
      for (int i = 0; i < attributeCount; i++)
      {
         store(attributes[i].getName(), new Integer(i));
      }

      int operationCount = (operations != null) ? operations.length : 0;
      for (int i = 0; i < operationCount; i++)
      {
         MBeanOperationInfo operation = operations[i];
         MBeanParameterInfo[] params = operation.getSignature();
         String[] signature = new String[params.length];
         for (int j = 0; j < signature.length; j++)
         {
            signature[j] = params[j].getType();
         }
         store(operation.getName(), signature, new Integer(i));
      }
   }

   public Integer lookup(String actionName, String[] signature)
   {
      String word = actionName;
      int wordh = word.hashCode();
      int wordpos = -1;
      int maxword = (signature != null) ? signature.length : 0;

      Node node = opRoot;
      Integer rval = null;
      OUTER_NODE: while (node != null)
      {
         if (wordh < node.hash)
         {
            node = node.loKid;
         }
         else if (wordh > node.hash)
         {
            node = node.hiKid;
         }
         else
         {
            for (int i = node.eqKid.length - 1; i > -1; i--)
            {
               if (word.equals(node.eqKid[i].val))
               {
                  if (++wordpos < maxword)
                  {
                     node = node.eqKid[i];
                     word = signature[wordpos];
                     wordh = word.hashCode();
                     continue OUTER_NODE;
                  }
                  else
                  {
                     rval = node.eqKid[i].code;
                     break OUTER_NODE;
                  }
               }
            }
         }
      }
      return rval;
   }

   public Integer lookup(String attrName)
   {
      int attrh = attrName.hashCode();

      Node node = atRoot;
      Integer rval = null;
      OUTER_NODE: while (node != null)
      {
         if (attrh < node.hash)
         {
            node = node.loKid;
         }
         else if (attrh > node.hash)
         {
            node = node.hiKid;
         }
         else
         {
            for (int i = node.eqKid.length - 1; i > -1; i--)
            {
               if (attrName.equals(node.eqKid[i].val))
               {
                  rval = node.eqKid[i].code;
                  break OUTER_NODE;
               }
            }
         }
      }
      return rval;
   }

   public void store(String mname, String[] signature, Integer code)
   {
      if (opRoot == null)
      {
         opRoot = createNode(mname);
         createValueNode(opRoot, mname);
      }

      int word = -1;
      int maxword = (signature != null) ? signature.length : 0;

      Node current = createOrGetNode(opRoot, mname);
      while (++word < maxword)
      {
         current = createOrGetNode(current, signature[word]);
      }

      current.code = code;
   }

   public void store(String attrName, Integer code)
   {
      Node current = null;

      if (atRoot == null)
      {
         atRoot = createNode(attrName);
         current = createValueNode(atRoot, attrName);
      }
      else
      {
         current = createOrGetNode(atRoot, attrName);
      }

      current.code = code;
   }

   protected Node createNode(String key)
   {
      Node h = new Node();
      h.hash = key.hashCode();
      h.val = key;
      return h;
   }

   protected Node createValueNode(Node parent, String key)
   {
      Node h = new Node();
      h.val = key;
      h.hash = key.hashCode();
      int insertAt = 0;
      if (parent.eqKid == null)
      {
         parent.eqKid = new Node[1];
      }
      else
      {
         Node[] old = parent.eqKid;
         insertAt = old.length;
         parent.eqKid = new Node[insertAt + 1];
         System.arraycopy(old, 0, parent.eqKid, 0, insertAt);
      }

      parent.eqKid[insertAt] = h;
      return h;
   }

   protected Node createOrGetNode(Node parent, String key)
   {
      Node realParent = parent;
      int keycode = key.hashCode();

      while (true)
      {
         if (keycode < realParent.hash)
         {
            if (realParent.loKid == null)
            {
               realParent.loKid = createNode(key);
               return createValueNode(realParent.loKid, key);
            }
            realParent = realParent.loKid;
         }
         else if (keycode > realParent.hash)
         {
            if (realParent.hiKid == null)
            {
               realParent.hiKid = createNode(key);
               return createValueNode(realParent.hiKid, key);
            }
            realParent = realParent.hiKid;
         }
         else
         {
            if (realParent.eqKid != null)
            {
               for (int i = 0; i < realParent.eqKid.length; i++)
               {
                  if (key.equals(realParent.eqKid[i].val))
                  {
                     return realParent.eqKid[i];
                  }
               }
            }
            return createValueNode(realParent, key);
         }
      }
   }


   public static class Node
   {
      public int hash = 0;
      public String val = null;

      public Node hiKid = null;
      public Node loKid = null;
      public Node[] eqKid = null;

      public Integer code = null;
   }
}
