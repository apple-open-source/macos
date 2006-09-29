# Mold Remover
#
# A short python script to remove old read mail from a pop3 mailserver.
# Distributed under the GNU GPL (http://www.gnu.org/copyleft/gpl.html)
# Dovetails with fetchmail with keep option.
# Run it as a cron job... Remember to stop fetchmail in the cron job
# before calling mold_remover, and to restart fetchmail after mold_remover
# e.g.: 
#
#	/etc/init.d/fetchmail stop >> /var/log/messages
#	/usr/bin/python /usr/local/bin/mold_remover.py >> /var/log/messages
#	/etc/init.d/fetchmail start >> /var/log/messages
#
# Version 0.3 by James Stone (jmstone@dsl.pipex.com)
# 15th March 2004
# 
# Changes:
# 0.1 25th September 2003 Original version
# 0.2 6th March 2004 Info added regarding use, and explicit mention of GPL made.
# 0.3 15th March 2004 days changed to list, 1 day changed from 24 to 23 hours.
# 
# please submit bug reports and code optimisations as you see fit!

import string
import poplib
import time

#user editable section

mailserver=["mail.server1","mail.server2"] #list of mailservers
login=["login1","login2"] #list of logins for corresponding mailserver
password=["pass1","pass2"] #list of passwords (note: chmod 700 for this script)
days=[2,3] #number of days to keep on server.
localuidlcache="/var/mail/.fetchmail-UIDL-cache" #fetchmail's UIDL cache
localuidldate="/var/mail/.UIDLDate" #mold remover's UIDL datefile

#end of user editable section

readfile=open(localuidlcache, 'r')
datefile=open(localuidldate, 'a+')
tempfile=open("/tmp/uidltmp", 'w+')
popuidllist=[] #list of undeleted uidls on all servers
totnum=0 #number of undeleted messages on all servers
connectedto=-1

#connect to each mailserver get all the new message UIDLs and delete any
#expired messages.

for a in range(len(mailserver)):
    connect=poplib.POP3(mailserver[a])
    connect.user(login[a])
    connect.pass_(password[a])
    connectedto=a
    number,size=connect.stat()
    totnum+=number
    for mesnum in range(number):
        messagedeleted=0
        datefile.seek(0)
        for uidldate in datefile:
            uidldatesplit=uidldate.split(' ')
            if(connectedto==int(uidldatesplit[2])):
                if (time.time()-float(uidldatesplit[1]))>(86400*days[a]-3600):
                    try:
                        recheckuidl=connect.uidl(mesnum+1)
                        recheckuidlsplit=recheckuidl.split(' ')
                        if (recheckuidlsplit[2]==uidldatesplit[0]):
                            print('deleting'+recheckuidlsplit[1])
                            print(connect.dele(recheckuidlsplit[1]))
                            messagedeleted=1
                            totnum-=1
                    except poplib.error_proto:
                        pass #skip over messages that have already been deleted.
        if not(messagedeleted):
            popuidllist.append(connect.uidl(mesnum+1)+' '+str(a))
    connect.quit()        


#get rid of lines in uidldate file corresponding to the messages that have been
#expired (and hopefully been deleted)

datefile.seek(0)
for uidldate in datefile:
    uidldatesplit=uidldate.split(' ')
    if not(time.time()-float(uidldatesplit[1]))>(86400*days[int(uidldatesplit[2])]):
        tempfile.write(uidldate)
datefile.close()
datefile=open(localuidldate,'w+')
tempfile.seek(0)
for line in tempfile:
        datefile.write(line)
datefile.close()
datefile=open(localuidldate,'a+')

#add date to uidl for any messages still on the server which have been read 
#(check in readfile) and store in local datefile.

for mesnum in range(totnum):    
            popuidl=popuidllist[mesnum]
            popuidlsplit=popuidl.split(' ')
            readfile.seek(0)
            for localuidl in readfile:
                if(localuidl.find(popuidlsplit[2])<>-1):
                    foundindatefile=0
                    datefile.seek(0)
                    for stored in datefile:
                        if (stored.find(popuidlsplit[2])<>-1):
                            foundindatefile=1
                    if not(foundindatefile):
                        datefile.write(popuidlsplit[2]+' '+str(time.time())+' '
                            +popuidlsplit[3]+'\n')
