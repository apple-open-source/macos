/*
 *  testagentclient.cpp
 *  SecurityAgent
 *
 *  Created by Conrad Sauerwald on Thu Oct 10 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>

#include "testagentclient.h"
#include <security_agent_client/agentclient.h>

#include <iostream>
#include <algorithm>

struct item_print : public unary_function<Authorization::AuthItemRef, void>
{
    item_print(ostream& out) : os(out), count(0) {}
    void operator() (Authorization::AuthItemRef x) 
    { 
        os << "item(" << count << ") : " << (*x).name() << " = " << 
            static_cast<char *>((*x).value().data) << endl; 
        ++count; 
    }
    ostream& os;
    int count;
};


#if  0
OSStatus testAZNAuth()
{
    OSStatus status;
    do {
        SecurityAgent::Client &client = *(new SecurityAgent::Client("com.apple.SecurityAgent"));

        status = client.create("AuthorizationAuthentication", "mwooahaha", 0); 
                                // plugin, mech, session
        if (status) break;
        {
            AuthValueVector args;
            AuthItemSet hints, context;

            status = client.invoke(args, hints, context);
        }
        if (status) break;

        AuthItemSet &returned_hints = client.hints();
        AuthItemSet &returned_context = client.context();

        cout << "Returned hints:" << endl;
        for_each(returned_hints.begin(), returned_hints.end(), item_print(cout));
        cout << "Returned context:" << endl;
        for_each(returned_context.begin(), returned_context.end(), item_print(cout));

        status = client.deactivate();
    } while (0);

    return status;
}

OSStatus testKCunlock()
{
    OSStatus status;
	SecurityAgent::Client *client;
	
    do {
        client = new SecurityAgent::Client("NewSecurityAgent");

        status = client.create("UnlockKeychain", "mwooahaha", 0); 
                                // plugin, mech, session
        if (status) break;
        {
            AuthValueVector args;
            AuthItemSet hints, context;

            status = client.invoke(args, hints, context);
        }
        if (status) break;

        AuthItemSet &returned_hints = client.hints();
        AuthItemSet &returned_context = client.context();

        cout << "Returned hints:" << endl;
        for_each(returned_hints.begin(), returned_hints.end(), item_print(cout));
        cout << "Returned context:" << endl;
        for_each(returned_context.begin(), returned_context.end(), item_print(cout));

		
    } while (0);
	
	status = client.destroy();
	delete client;

    return status;
}
#endif

OSStatus testMechanism(const char *plugin, const char *mechanism/*int steps*/)
{
    OSStatus status;
	int steps = 10;

	SecurityAgent::Client &client = *(new SecurityAgent::Client());

    do {

		steps--; if (steps <= 0) break;

        status = client.create(plugin, mechanism, 0); 
                                // plugin, mech, session
        if (status) break;
		steps--; if (steps <= 0) break;

        {
            Authorization::AuthValueVector args;
            Authorization::AuthItemSet hints, context;

            status = client.invoke(args, hints, context);
        }
        if (status) break;

		steps--; if (steps <= 0) break;

        Authorization::AuthItemSet &returned_hints = client.hints();
        Authorization::AuthItemSet &returned_context = client.context();

        cout << "Returned hints:" << endl;
        for_each(returned_hints.begin(), returned_hints.end(), item_print(cout));
        cout << "Returned context:" << endl;
        for_each(returned_context.begin(), returned_context.end(), item_print(cout));

		//status = client.deactivate();
		steps--; if (steps <= 0) break;

		// don't care if this fails

    } while (0);
		
	client.destroy(); // regardless
		
    return status;
}

OSStatus testBuiltin(const char *mechanism/*int steps*/)
{
	return testMechanism("builtin", mechanism);
}


int main(int argc, char *argv[])
{
    OSStatus status;
    do {
        //status = testAZNAuth();
        //if (status) break;
        //status = testKCunlock();
        //if (status) break;
	  //testBuiltin("authenticate");
		testMechanism("loginwindow_builtin", "login");
		
    } while (0);

    return status;
}

