package org.jboss.net.sockets;


public interface RMIMultiSocket extends java.rmi.Remote
{
   public Object invoke (long methodHash, Object[] args) throws Exception;
}
