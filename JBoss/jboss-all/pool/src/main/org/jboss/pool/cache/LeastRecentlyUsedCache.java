/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool.cache;

import java.util.*;

import org.jboss.logging.Logger;

/**
 *  A Least Recently Used cache implementation. The object in the cache that was
 *  least recently used is dropped when the cache is full and a new request
 *  comes in. The implementation uses a linked list (to track recentness) and a
 *  HashMap (to navigate quickly).
 *
 * @author     Aaron Mulder ammulder@alumni.princeton.edu
 * @created    August 18, 2001
 */
public class LeastRecentlyUsedCache implements ObjectCache {
   private Object   lock = new Object();
   private HashMap  keyMap = new HashMap();
   private Node     mostRecentNode, leastRecentNode;
   private int      size;
   private int      maxSize;
   private CachedObjectFactory factory;

   private Logger log = Logger.getLogger( LeastRecentlyUsedCache.class );


   public LeastRecentlyUsedCache( CachedObjectFactory factory, int maxSize ) {
      this.factory = factory;
      this.maxSize = maxSize;
   }

   public void setSize( int maxSize ) {
      this.maxSize = maxSize;
      checkMaxSize();
   }

   public Object getObject( Object key ) {
      return getObject( key, false );
   }

   public Object useObject( Object key ) {
      return getObject( key, true );
   }

   public void returnObject( Object key, Object value ) {
      Object pooled = keyMap.get( key );
      Object source = factory.translateObject( value );

      if ( pooled == null ) {
         factory.deleteObject( source );
      } else if ( pooled instanceof Node ) {
         Node node = ( Node )pooled;
         if ( node.data == source ) {
            try {
               node.setUsed( false );
            } catch ( ConcurrentModificationException e ) {
            }
         } else {
            factory.deleteObject( source );
         }
      } else if ( pooled instanceof LinkedList ) {
         for ( Iterator it = ( ( LinkedList )pooled ).iterator(); it.hasNext();  ) {
            Node node = ( Node )it.next();
            if ( node.data == source ) {
               try {
                  node.setUsed( false );
               } catch ( ConcurrentModificationException e ) {
               }
               return;
            }
         }
         factory.deleteObject( source );
      } else {
         throw new Error( "LRU Cache Assertion Failure: Wrong class '" + pooled.getClass().getName() + "' in keyMap!" );
      }
   }

   public void removeObjects( Object key ) {
      synchronized ( lock ) {
         Object value = keyMap.get( key );
         if ( value == null ) {
            return;
         } else if ( value instanceof Node ) {
            removeNode( ( Node )value );
         } else if ( value instanceof LinkedList ) {
            LinkedList list = ( LinkedList )value;
            int max = list.size();
            for ( int i = 0; i < max; i++ ) {
               removeNode( ( Node )list.get( 0 ) );
            }
         }
      }
   }

   public void close() {
      synchronized ( lock ) {
         Node current = leastRecentNode;
         Node next = current == null ? null : current.moreRecent;
         while ( current != null ) {
            removeNode( current );
            current = next;
            next = current == null ? null : current.moreRecent;
         }
      }
   }

   private Object getObject( Object key, boolean use ) {
      Node node = null;
      try {
         Object value = keyMap.get( key );
         if ( value == null ) {
            node = addObject( key );
         } else if ( value instanceof Node ) {
            node = ( Node )value;
            if ( !node.used ) {
               makeMostRecent( node );
            } else {
               node = addObject( key );
            }
         } else if ( value instanceof LinkedList ) {
            for ( Iterator it = ( ( LinkedList )value ).iterator(); it.hasNext();  ) {
               node = ( Node )it.next();
               if ( !node.used ) {
                  makeMostRecent( node );
                  break;
               } else {
                  node = null;
               }
            }
            if ( node == null ) {
               node = addObject( key );
            }
         } else {
            throw new Error( "LRU Cache Assertion Failure: Wrong class '" + value.getClass().getName() + "' in keyMap!" );
         }
         if ( use ) {
            node.setUsed( true );
         }
      } catch ( ConcurrentModificationException e ) {
         return getObject( key, use );
      }
      return factory.prepareObject( node.data );
   }

   private Node addObject( Object key ) {
      Object data;
      try {
         data = factory.createObject( key );
      } catch ( Exception e ) {
         log.error( "Could not add object", e );
         return null;
      }
      Node result;
      synchronized ( lock ) {
         if ( mostRecentNode == null ) {
            result = new Node( null, null, data );
            mostRecentNode = result;
            leastRecentNode = result;
         } else {
            result = new Node( mostRecentNode, null, data );
            mostRecentNode.moreRecent = result;
            mostRecentNode = result;
         }
         ++size;
         checkMaxSize();
      }

      // Put the key/value(s) and node/key in the map
      Object value = keyMap.get( key );
      if ( value == null ) {
         keyMap.put( key, result );
      } else if ( value instanceof LinkedList ) {
         ( ( LinkedList )value ).add( result );
      } else if ( value instanceof Node ) {
         LinkedList list = new LinkedList();
         list.add( value );
         list.add( result );
         keyMap.put( key, list );
      } else {
         throw new Error( "LRU Cache Assertion Failure: Wrong class '" + value.getClass().getName() + "' in keyMap!" );
      }
      keyMap.put( result, key );
      return result;
   }

   private void checkMaxSize() {
      if ( maxSize <= 0 ) {
         return;
      }
      while ( size > maxSize ) {
         Node drop = leastRecentNode;
         leastRecentNode = drop.moreRecent;
         leastRecentNode.lessRecent = null;
         --size;

         removeNode( drop );
      }
   }

   private void makeMostRecent( Node node ) {
      synchronized ( lock ) {
         if ( node.moreRecent == null ) {
            if ( mostRecentNode != node ) {
               throw new ConcurrentModificationException();
            }
            return;
         }

         // Prepare surrounding nodes
         Node previous = node.moreRecent;
         Node next = node.lessRecent;
         previous.lessRecent = next;
         if ( next == null ) {
            leastRecentNode = previous;
         } else {
            next.moreRecent = previous;
         }

         // Prepare node and new 2nd most recent
         node.moreRecent = null;
         node.lessRecent = mostRecentNode;
         mostRecentNode.moreRecent = node;
         mostRecentNode = node;
      }
   }

   // This should be called while holding the lock
   private void removeNode( Node node ) {
      boolean used = node.used;
      if ( !used ) {
         node.used = true;
      }
      Object key = keyMap.remove( node );

      Object value = keyMap.get( key );
      if ( value instanceof Node ) {
         keyMap.remove( key );
      } else if ( value instanceof LinkedList ) {
         LinkedList list = ( LinkedList )value;
         Iterator it = list.iterator();
         while ( it.hasNext() ) {
            Node current = ( Node )it.next();
            if ( current == node ) {
               it.remove();
               break;
            }
         }
         if ( list.size() == 1 ) {
            keyMap.put( key, list.get( 0 ) );
         }
      } else {
         throw new Error( "LRU Cache Assertion Failure: Wrong class '" + value.getClass().getName() + "' in keyMap!" );
      }
      if ( !used ) {
         factory.deleteObject( node.data );
      }
      node.moreRecent = null;
      node.lessRecent = null;
   }

   /**
    * @created    August 18, 2001
    */
   private class Node {
      Node          lessRecent;
      Node          moreRecent;
      Object        data;
      boolean       used = false;

      public Node( Node lessRecent, Node moreRecent, Object data ) {
         this( lessRecent, moreRecent, data, false );
      }

      public Node( Node lessRecent, Node moreRecent, Object data, boolean used ) {
         this.lessRecent = lessRecent;
         this.moreRecent = moreRecent;
         this.data = data;
         this.used = used;
      }

      public synchronized void setUsed( boolean used ) {
         if ( this.used == used ) {
            throw new ConcurrentModificationException();
         }
         this.used = used;
      }
   }

   /*
    * This is for testing only
    * private void printList() {
    * System.out.print("Fwd Nodes:");
    * Node node = mostRecentNode;
    * for(int i=0; i<size; i++) {
    * System.out.print(" "+node.data);
    * node = node.lessRecent;
    * }
    * System.out.println();
    * Stack stack = new Stack();
    * node = leastRecentNode;
    * for(int i=0; i<size; i++) {
    * stack.push(node.data);
    * node = node.moreRecent;
    * }
    * System.out.print("Rev Nodes:");
    * while(!stack.isEmpty())
    * System.out.print(" "+stack.pop());
    * System.out.println();
    * }
    * public static void main(String args[]) {
    * try {
    * java.io.BufferedReader in = new java.io.BufferedReader(new java.io.InputStreamReader(System.in));
    * LeastRecentlyUsedCache cache = new LeastRecentlyUsedCache(new CachedObjectFactory() {
    * public Object createObject(Object identifier) {
    * return "v"+identifier;
    * }
    * public void deleteObject(Object object) {
    * System.out.println("Deleting Object "+object);
    * }
    * }, 5);
    * THIS TESTS getObject
    * while(true) {
    * cache.printList();
    * System.out.print("> ");
    * System.out.flush();
    * String key = in.readLine();
    * if(key.equalsIgnoreCase("quit") || key.equalsIgnoreCase("exit"))
    * return;
    * Object value = cache.getObject(key);
    * System.out.println("Got Value: "+value);
    * }
    *
    * THIS TESTS useObject AND returnObject
    * Object lastKey = null, last = null;
    * while(true) {
    * cache.printList();
    * System.out.print("> ");
    * System.out.flush();
    * String key = in.readLine();
    * if(key.equalsIgnoreCase("quit") || key.equalsIgnoreCase("exit"))
    * return;
    * Object value = cache.useObject(key);
    * if(last != null) {
    * cache.returnObject(lastKey, last);
    * }
    * lastKey = key;
    * last = value;
    * System.out.println("Got Value: "+value);
    * }
    *
    * } catch(Throwable e) {
    * e.printStackTrace();
    * }
    * }
    */
}
