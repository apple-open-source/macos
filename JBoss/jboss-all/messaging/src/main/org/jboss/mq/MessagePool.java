/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

/**
 *	This class provides a pool of SpyMessages.
 *
 * This is an very simple implementation first up.
 *
 *	@author David Maplesden (David.Maplesden@orion.co.nz)
 */
public class MessagePool {

   //static flag which turns off pooling altogether if false.
   public static final boolean POOL = true;

   //no point growing pools too much, the max size any one pool will get
   public static final int MAX_POOL_SIZE = 10*1000;

   protected static java.util.ArrayList messagePool = new java.util.ArrayList();
   protected static java.util.ArrayList bytesPool = new java.util.ArrayList();
   protected static java.util.ArrayList mapPool = new java.util.ArrayList();
   protected static java.util.ArrayList streamPool = new java.util.ArrayList();
   protected static java.util.ArrayList objectPool = new java.util.ArrayList();
   protected static java.util.ArrayList textPool = new java.util.ArrayList();
   protected static java.util.ArrayList encapPool = new java.util.ArrayList();

   /**
    * Gets a message from the pools.
    */
   public static SpyMessage getMessage(){
      if(POOL){
         synchronized(messagePool){
            if(!messagePool.isEmpty())
               return (SpyMessage) messagePool.remove(messagePool.size()-1);
         }
      }
      return new SpyMessage();
   }

   /**
    * Gets a bytes message from the pools.
    */
   public static SpyBytesMessage getBytesMessage(){
      if(POOL){
         synchronized(bytesPool){
            if(!bytesPool.isEmpty())
               return (SpyBytesMessage) bytesPool.remove(bytesPool.size()-1);
         }
      }
      return new SpyBytesMessage();
   }

   /**
    * Gets a map message from the pools.
    */
   public static SpyMapMessage getMapMessage(){
      if(POOL){
         synchronized(mapPool){
            if(!mapPool.isEmpty())
               return (SpyMapMessage) mapPool.remove(mapPool.size()-1);
         }
      }
      return new SpyMapMessage();
   }

   /**
    * Gets a stream message from the pools.
    */
   public static SpyStreamMessage getStreamMessage(){
      if(POOL){
         synchronized(streamPool){
            if(!streamPool.isEmpty())
               return (SpyStreamMessage) streamPool.remove(streamPool.size()-1);
         }
      }
      return new SpyStreamMessage();
   }

   /**
    * Gets a object message from the pools.
    */
   public static SpyObjectMessage getObjectMessage(){
      if(POOL){
         synchronized(objectPool){
            if(!objectPool.isEmpty())
               return (SpyObjectMessage) objectPool.remove(objectPool.size()-1);
         }
      }
      return new SpyObjectMessage();
   }

   /**
    * Gets a text message from the pools.
    */
   public static SpyTextMessage getTextMessage(){
      if(POOL){
         synchronized(textPool){
            if(!textPool.isEmpty())
               return (SpyTextMessage) textPool.remove(textPool.size()-1);
         }
      }
      return new SpyTextMessage();
   }

   /**
    * Gets a encapsulated message from the pools.
    */
   public static SpyEncapsulatedMessage getEncapsulatedMessage(){
      if(POOL){
         synchronized(encapPool){
            if(!encapPool.isEmpty())
               return (SpyEncapsulatedMessage) encapPool.remove(encapPool.size()-1);
         }
      }
      return new SpyEncapsulatedMessage();
   }

   /**
    * Releases a SpyMessage back to the pools for reuse.
    */
   public static void releaseMessage(SpyMessage message){
      if(!POOL){
         return;
      }else{
         if(message == null)
            return;
         try{
            message.clearMessage();
         }catch(javax.jms.JMSException e){
            //unable to re-use message
            return;
         }
         if(message instanceof SpyTextMessage){
            synchronized(textPool){
               if(textPool.size() < MAX_POOL_SIZE)
                  textPool.add(message);
            }
         }else if(message instanceof SpyEncapsulatedMessage){
            //must test for encap mess before object mess because encap mess extends object mess
            synchronized(encapPool){
               if(encapPool.size() < MAX_POOL_SIZE)
                  encapPool.add(message);
            }
         }else if(message instanceof SpyObjectMessage){
            synchronized(objectPool){
               if(objectPool.size() < MAX_POOL_SIZE)
                  objectPool.add(message);
            }
         }else if(message instanceof SpyBytesMessage){
            synchronized(bytesPool){
               if(bytesPool.size() < MAX_POOL_SIZE)
                  bytesPool.add(message);
            }
         }else if(message instanceof SpyMapMessage){
            synchronized(mapPool){
               if(mapPool.size() < MAX_POOL_SIZE)
                  mapPool.add(message);
            }
         }else if(message instanceof SpyStreamMessage){
            synchronized(streamPool){
               if(streamPool.size() < MAX_POOL_SIZE)
                  streamPool.add(message);
            }
         }else{
            //plain old SpyMessage
            synchronized(messagePool){
               if(messagePool.size() < MAX_POOL_SIZE)
                  messagePool.add(message);
            }
         }
      }
   }
}