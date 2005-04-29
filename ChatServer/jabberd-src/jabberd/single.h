#define SINGLE_STARTUP \
    home = ghash_get(cmd__line,"H"); \
    if(ghash_get(cmd__line,"h") == NULL || (home == NULL && (ghash_get(cmd__line,"s") == NULL || ghash_get(cmd__line,"l") == NULL))) \
    { /* require hostname, and either a HOME or the spool and logs specifically */ \
        fprintf(stderr, "\
Usage:\n\
 jabberd -h hostname -s /var/spool/jabber -l /var/log/jabber.log &\n\
Required Parameters:\n\
 -h \t\t A valid hostname that this server is accessible via\n\
 -s \t\t A folder in which user account data can be stored\n\
 -l \t\t The name of the log file\n\
Optional Parameters:\n\
 -H \t\t A 'home' folder, which can be used instead of the -s and -l above as the parent folder\n\
 -c \t\t Location of a configuration file to use instead of the built-in configuration\n\
 -v \t\t server version\n\
 -D \t\t Enable verbose debug output\n"); \
        exit(0); \
    }
    
#define SINGLE_CONFIG "\
<jabber> \
  <service id='sessions'> \
    <host><jabberd:cmdline flag='h'>localhost</jabberd:cmdline></host> \
    <jsm xmlns='jabber:config:jsm'> \
      <register><instructions>Choose a username and password to register with this server.</instructions><name/><email/></register> \
      <update><jabberd:cmdline flag='h'>localhost</jabberd:cmdline></update> \
      <vcard2jud/> \
      <browse><service type='jud' jid='users.jabber.org' name='Jabber User Directory'><ns>jabber:iq:search</ns><ns>jabber:iq:register</ns></service></browse> \
    </jsm> \
    <load main='jsm'> \
      <jsm>./jsm/jsm.so</jsm> \
      <mod_roster>./jsm/jsm.so</mod_roster> \
      <mod_time>./jsm/jsm.so</mod_time> \
      <mod_vcard>./jsm/jsm.so</mod_vcard> \
      <mod_last>./jsm/jsm.so</mod_last> \
      <mod_version>./jsm/jsm.so</mod_version> \
      <mod_agents>./jsm/jsm.so</mod_agents> \
      <mod_browse>./jsm/jsm.so</mod_browse> \
      <mod_offline>./jsm/jsm.so</mod_offline> \
      <mod_presence>./jsm/jsm.so</mod_presence> \
      <mod_auth_plain>./jsm/jsm.so</mod_auth_plain> \
      <mod_auth_digest>./jsm/jsm.so</mod_auth_digest> \
      <mod_auth_0k>./jsm/jsm.so</mod_auth_0k> \
      <mod_log>./jsm/jsm.so</mod_log> \
      <mod_register>./jsm/jsm.so</mod_register> \
      <mod_xml>./jsm/jsm.so</mod_xml> \
    </load> \
  </service> \
  <xdb id='xdb'> \
    <host/> \
    <load><xdb_file>./xdb_file/xdb_file.so</xdb_file></load> \
    <xdb_file xmlns='jabber:config:xdb_file'><spool><jabberd:cmdline flag='s'>./spool</jabberd:cmdline></spool></xdb_file> \
  </xdb> \
  <service id='c2s'> \
    <load><pthsock_client>./pthsock/pthsock_client.so</pthsock_client></load> \
    <pthcsock xmlns='jabber:config:pth-csock'> \
      <authtime/> \
      <rate time='5' points='25'/> \
      <karma> \
        <init>10</init> \
        <max>10</max> \
        <inc>1</inc> \
        <dec>1</dec> \
        <restore>10</restore> \
        <penalty>-6</penalty> \
      </karma> \
      <ip port='5222'/> \
    </pthcsock> \
  </service> \
  <log id='logger'> \
    <host/> \
    <format>%d: [%t] (%h): %s</format> \
    <file><jabberd:cmdline flag='l'>jabber.log</jabberd:cmdline></file> \
    <stderr/> \
  </log> \
  <service id='dnsrv'> \
    <host/> \
    <load><dnsrv>./dnsrv/dnsrv.so</dnsrv></load> \
    <dnsrv xmlns='jabber:config:dnsrv'> \
    	<resend service='_jabber._tcp'>s2s</resend> \
    	<resend>s2s</resend>  \
    </dnsrv> \
  </service> \
  <service id='s2s'> \
    <load><pthsock_server>./pthsock/pthsock_server.so</pthsock_server></load> \
    <pthssock xmlns='jabber:config:pth-ssock'> \
      <legacy/> \
      <rate time='5' points='25'/> \
      <karma> \
        <init>50</init> \
        <max>50</max> \
        <inc>4</inc> \
        <dec>1</dec> \
        <restore>50</restore> \
        <penalty>-5</penalty> \
      </karma> \
      <ip port='5269'/> \
    </pthssock> \
  </service> \
</jabber>"
