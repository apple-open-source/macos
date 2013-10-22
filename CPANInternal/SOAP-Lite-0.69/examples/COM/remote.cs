// -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

// Lots of thanks to Tony Hong (xmethods.net) for provided help and examples

// Connect to remote SOAP server

using System;
using System.Reflection;

public class test {
  public static void Main() {
    Type typ = Type.GetTypeFromProgID("SOAP.Lite");
    object obj = Activator.CreateInstance(typ);
    object soaplite = typ.InvokeMember("new",BindingFlags.InvokeMethod,null,obj,null);

    Object[] uri = {"urn:xmethodsInterop"};
    Object[] proxy = {"http://services.xmethods.net:80/perl/soaplite.cgi"};

    typ.InvokeMember("uri",BindingFlags.InvokeMethod,null,soaplite,uri);
    typ.InvokeMember("proxy",BindingFlags.InvokeMethod,null,soaplite,proxy);

    object [] input = {"Hello"};

    object resultObject = typ.InvokeMember("echoString",BindingFlags.InvokeMethod,null,soaplite,input);
    Console.WriteLine(typ.InvokeMember("result",BindingFlags.InvokeMethod,null,resultObject,null));
  }
}
