/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test;

import java.io.IOException;
import java.rmi.AlreadyBoundException;
import java.rmi.RemoteException;
import java.rmi.registry.LocateRegistry;
import java.rmi.registry.Registry;

import org.jboss.security.srp.SerialObjectStore;
import org.jboss.security.srp.SRPRemoteServer;

/** An RMI application that creates a SRPRemoteServer instance and
exports it on the standard RMI register 1099 port. It creates a
SerialObjectStore as the SRPVerifierStore for the SRPRemoteServer.

@author Scott.Stark@jboss.org
@version $Revision: 1.2.4.1 $
*/
public class SRPServerImpl
{
    SerialObjectStore store;

    void run() throws IOException, AlreadyBoundException, RemoteException
    {
        store = new SerialObjectStore();
        SRPRemoteServer server = new SRPRemoteServer(store);
        Registry reg = LocateRegistry.createRegistry(Registry.REGISTRY_PORT);
        reg.bind("SimpleSRPServer", server);
    }

    public static void main(String[] args) throws Exception
    {
        SRPServerImpl server = new SRPServerImpl();
        server.run();
    }
}
