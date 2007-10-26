/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CLocalPlugin
 */


#ifndef _CDSLocalPlugin_
#define _CDSLocalPlugin_	1

#include <CoreFoundation/CoreFoundation.h>
#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "CDSServerModule.h"
#include "SharedConsts.h"
#include "PluginData.h"
#include "BaseDirectoryPlugin.h"
#include "CDSAuthParams.h"

#define LOG_REQUEST_TIMES		0
#define LOG_MAPPINGS			0
#define FILE_ACCESS_INDEXING	1

#define kDefaultLocalAttrMappings		"<dict>                    \
	<key>dsAttrTypeStandard:AddressLine1</key>                     \
	<string>address1</string>                                      \
	<key>dsAttrTypeStandard:AddressLine2</key>                     \
	<string>address2</string>                                      \
	<key>dsAttrTypeStandard:AddressLine3</key>                     \
	<string>address3</string>                                      \
	<key>dsAttrTypeStandard:AdminLimits</key>                      \
	<string>admin_limits</string>                                  \
	<key>dsAttrTypeStandard:AdminStatus</key>                      \
	<string>AdminStatus</string>                                   \
	<key>dsAttrTypeStandard:AllNames</key>                         \
	<string>dsAttrTypeStandard:AllNames</string>                   \
	<key>dsAttrTypeStandard:AlternateDatastoreLocation</key>       \
	<string>alternatedatastorelocation</string>                    \
	<key>dsAttrTypeStandard:AppleAliasData</key>                   \
	<string>alias_data</string>                                    \
	<key>dsAttrTypeStandard:AppleMetaNodeLocation</key>            \
	<string>dsAttrTypeStandard:AppleMetaNodeLocation</string>      \
	<key>dsAttrTypeStandard:AreaCode </key>                        \
	<string>areacode</string>                                      \
	<key>dsAttrTypeStandard:AuthenticationAuthority</key>          \
	<string>authentication_authority</string>                      \
	<key>dsAttrTypeStandard:AuthenticationHint</key>               \
	<string>hint</string>                                          \
	<key>dsAttrTypeStandard:Automount</key>                        \
	<string>autmount</string>                                      \
	<key>dsAttrTypeStandard:AutomountInformation</key>             \
	<string>autmountInformation</string>                           \
	<key>dsAttrTypeStandard:AutomountKey</key>                     \
	<string>autmountKey</string>                                   \
	<key>dsAttrTypeStandard:AutomountMap</key>                     \
	<string>automountMap</string>                                  \
	<key>dsAttrTypeStandard:Birthday</key>                         \
	<string>birthday</string>                                      \
	<key>dsAttrTypeStandard:BootFile</key>                         \
	<string>bootfile</string>                                      \
	<key>dsAttrTypeStandard:BootParams</key>                       \
	<string>bootparams</string>                                    \
	<key>dsAttrTypeStandard:Building</key>                         \
	<string>building</string>                                      \
	<key>dsAttrTypeStandard:Capabilities</key>                     \
	<string>capabilities</string>                                  \
	<key>dsAttrTypeStandard:Capacity</key>                         \
	<string>capacity</string>                                      \
	<key>dsAttrTypeStandard:Category</key>                         \
	<string>category</string>                                      \
	<key>dsAttrTypeStandard:Change</key>                           \
	<string>change</string>                                        \
	<key>dsAttrTypeStandard:City</key>                             \
	<string>city</string>                                          \
	<key>dsAttrTypeStandard:Comment</key>                          \
	<string>comment</string>                                       \
	<key>dsAttrTypeStandard:Company</key>                          \
	<string>company</string>                                       \
	<key>dsAttrTypeStandard:ComputerAlias</key>                    \
	<string>computeralias</string>                                 \
	<key>dsAttrTypeStandard:Computers</key>                        \
	<string>computers</string>                                     \
	<key>dsAttrTypeStandard:ContactGUID</key>                      \
	<string>contactguid</string>                                   \
	<key>dsAttrTypeStandard:ContactPerson</key>                    \
	<string>owner</string>                                         \
	<key>dsAttrTypeStandard:CopyTimestamp</key>                    \
	<string>copy_timestamp</string>                                \
	<key>dsAttrTypeStandard:Country</key>                          \
	<string>country</string>                                       \
	<key>dsAttrTypeStandard:CreationTimestamp</key>                \
	<string>creationtimestamp</string>                             \
	<key>dsAttrTypeStandard:DNSDomain</key>                        \
	<string>domain</string>                                        \
	<key>dsAttrTypeStandard:DNSName</key>                          \
	<string>dnsname</string>                                       \
	<key>dsAttrTypeStandard:DNSNameServer</key>                    \
	<string>nameserver</string>                                    \
	<key>dsAttrTypeStandard:DataStamp</key>                        \
	<string>data_stamp</string>                                    \
	<key>dsAttrTypeStandard:DateRecordCreated</key>                \
	<string>dateCreate</string>                                    \
	<key>dsAttrTypeStandard:Department</key>                       \
	<string>department</string>                                    \
	<key>dsAttrTypeStandard:EMailAddress</key>                     \
	<string>mail</string>                                          \
	<key>dsAttrTypeStandard:EMailContacts</key>                    \
	<string>emailcontacts</string>                                 \
	<key>dsAttrTypeStandard:ENetAddress</key>                      \
	<string>en_address</string>                                    \
	<key>dsAttrTypeStandard:Expire</key>                           \
	<string>expire</string>                                        \
	<key>dsAttrTypeStandard:FAXNumber</key>                        \
	<string>faxnumber</string>                                     \
	<key>dsAttrTypeStandard:FirstName</key>                        \
	<string>firstname</string>                                     \
	<key>dsAttrTypeStandard:GeneratedUID</key>                     \
	<string>generateduid</string>                                  \
	<key>dsAttrTypeStandard:Group</key>                            \
	<string>groups</string>                                        \
	<key>dsAttrTypeStandard:GroupMembers</key>                     \
	<string>groupmembers</string>                                  \
	<key>dsAttrTypeStandard:GroupMembership</key>                  \
	<string>users</string>                                         \
	<key>dsAttrTypeStandard:GroupServices</key>                    \
	<string>groupservices</string>                                 \
	<key>dsAttrTypeStandard:HTML</key>                             \
	<string>htmldata</string>                                      \
	<key>dsAttrTypeStandard:HomeDirectory</key>                    \
	<string>home_loc</string>                                      \
	<key>dsAttrTypeStandard:HomeDirectoryQuota</key>               \
	<string>homedirectoryquota</string>                            \
	<key>dsAttrTypeStandard:HomeDirectorySoftQuota</key>           \
	<string>homedirectorysoftquota</string>                        \
	<key>dsAttrTypeStandard:HomeLocOwner</key>                     \
	<string>home_loc_owner</string>                                \
	<key>dsAttrTypeStandard:IMHandle</key>                         \
	<string>imhandle</string>                                      \
	<key>dsAttrTypeStandard:IPAddress</key>                        \
	<string>ip_address</string>                                    \
	<key>dsAttrTypeStandard:IPv6Address</key>                      \
	<string>ipv6_address</string>                                  \
	<key>dsAttrTypeStandard:InetAlias</key>                        \
	<string>InetAlias</string>                                     \
	<key>dsAttrTypeStandard:JPEGPhoto</key>                        \
	<string>jpegphoto</string>                                     \
	<key>dsAttrTypeStandard:JobTitle</key>                         \
	<string>jobtitle</string>                                      \
	<key>dsAttrTypeStandard:KDCAuthKey</key>                       \
	<string>kdcauthkey</string>                                    \
	<key>dsAttrTypeStandard:KDCConfigData</key>                    \
	<string>kdcconfigdata</string>                                 \
	<key>dsAttrTypeStandard:Keywords</key>                         \
	<string>keywords</string>                                      \
	<key>dsAttrTypeStandard:LastName</key>                         \
	<string>lastname</string>                                      \
	<key>dsAttrTypeStandard:Location</key>                         \
	<string>location</string>                                      \
	<key>dsAttrTypeStandard:MetaAutomountMap</key>                 \
	<string>dsAttrTypeStandard:MetaAutomountMap</string>           \
	<key>dsAttrTypeStandard:MCXFlags</key>                         \
	<string>mcx_flags</string>                                     \
	<key>dsAttrTypeStandard:MCXSettings</key>                      \
	<string>mcx_settings</string>                                  \
	<key>dsAttrTypeStandard:MIME</key>                             \
	<string>mime</string>                                          \
	<key>dsAttrTypeStandard:MachineServes</key>                    \
	<string>serves</string>                                        \
	<key>dsAttrTypeStandard:MailAttribute</key>                    \
	<string>applemail</string>                                     \
	<key>dsAttrTypeStandard:MapCoordinates</key>                   \
	<string>mapcoordinates</string>                                \
	<key>dsAttrTypeStandard:Member</key>                           \
	<string>users</string>                                         \
	<key>dsAttrTypeStandard:MiddleName</key>                       \
	<string>middlename</string>                                    \
	<key>dsAttrTypeStandard:MobileNumber</key>                     \
	<string>mobilenumber</string>                                  \
	<key>dsAttrTypeStandard:ModificationTimestamp</key>            \
	<string>modificationtimestamp</string>                         \
	<key>dsAttrTypeStandard:NBPEntry</key>                         \
	<string>NBPEntry</string>                                      \
	<key>dsAttrTypeStandard:NFSHomeDirectory</key>                 \
	<string>home</string>                                          \
	<key>dsAttrTypeStandard:NamePrefix</key>                       \
	<string>nameprefix</string>                                    \
	<key>dsAttrTypeStandard:NameSuffix</key>                       \
	<string>namesuffix</string>                                    \
	<key>dsAttrTypeStandard:NeighborhoodAlias</key>                \
	<string>neighborhoodalias</string>                             \
	<key>dsAttrTypeStandard:NeighborhoodType</key>                 \
	<string>neighborhoodtype</string>                              \
	<key>dsAttrTypeStandard:NestedGroups</key>                     \
	<string>nestedgroups</string>                                  \
	<key>dsAttrTypeStandard:NetGroups</key>                        \
	<string>netgroups</string>                                     \
	<key>dsAttrTypeStandard:NetworkView</key>                      \
	<string>networkview</string>                                   \
	<key>dsAttrTypeStandard:NickName</key>                         \
	<string>nickname</string>                                      \
	<key>dsAttrTypeStandard:NodePathXMLPlist</key>                 \
	<string>nodepathxmlplist</string>                              \
	<key>dsAttrTypeStandard:Note</key>                             \
	<string>note</string>                                          \
	<key>dsAttrTypeStandard:Occupation</key>                       \
	<string>occupation</string>                                    \
	<key>dsAttrTypeStandard:OrganizationName</key>                 \
	<string>orgname</string>                                       \
	<key>dsAttrTypeStandard:OriginalHomeDirectory</key>            \
	<string>original_home_loc</string>                             \
	<key>dsAttrTypeStandard:OriginalNFSHomeDirectory</key>         \
	<string>original_home</string>                                 \
	<key>dsAttrTypeStandard:OriginalNodeName</key>                 \
	<string>original_node_name</string>                            \
	<key>dsAttrTypeStandard:OwnerGUID</key>                        \
	<string>ownerguid</string>                                     \
	<key>dsAttrTypeStandard:PGPPublicKey</key>                     \
	<string>pgppublickey</string>                                  \
	<key>dsAttrTypeStandard:PagerNumber</key>                      \
	<string>pagernumber</string>                                   \
	<key>dsAttrTypeStandard:Password</key>                         \
	<string>passwd</string>                                        \
	<key>dsAttrTypeStandard:PasswordPlus</key>                     \
	<string>passwd-plus</string>                                   \
	<key>dsAttrTypeStandard:PasswordPolicyOptions</key>            \
	<string>passwordpolicyoptions</string>                         \
	<key>dsAttrTypeStandard:PasswordServerList</key>               \
	<string>passwordserverlist</string>                            \
	<key>dsAttrTypeStandard:PasswordServerLocation</key>           \
	<string>passwordserverlocation</string>                        \
	<key>dsAttrTypeStandard:PhoneContacts</key>                    \
	<string>phonecontacts</string>                                 \
	<key>dsAttrTypeStandard:HomePhoneNumber</key>					\
	<string>homephonenumber</string>								\
	<key>dsAttrTypeStandard:PrimaryComputerList</key>				\
	<string>primarycomputerlist</string>							\
	<key>dsAttrTypeStandard:PhoneNumber</key>                      \
	<string>phonenumber</string>                                   \
	<key>dsAttrTypeStandard:Picture</key>                          \
	<string>picture</string>                                       \
	<key>dsAttrTypeStandard:Port</key>                             \
	<string>port</string>                                          \
	<key>dsAttrTypeStandard:PostalAddress</key>                    \
	<string>postaladdress</string>                                 \
	<key>dsAttrTypeStandard:PostalAddressContacts</key>            \
	<string>postaladdresscontacts</string>                         \
	<key>dsAttrTypeStandard:PostalCode</key>                       \
	<string>zip</string>                                           \
	<key>dsAttrTypeStandard:PresetUserIsAdmin</key>                \
	<string>preset_user_is_admin</string>                          \
	<key>dsAttrTypeStandard:PrimaryGroupID</key>                   \
	<string>gid</string>                                           \
	<key>dsAttrTypeStandard:PrintServiceInfoText</key>             \
	<string>PrintServiceInfoText</string>                          \
	<key>dsAttrTypeStandard:PrintServiceInfoXML</key>              \
	<string>PrintServiceInfoXML</string>                           \
	<key>dsAttrTypeStandard:PrintServiceUserData</key>             \
	<string>appleprintservice</string>                             \
	<key>dsAttrTypeStandard:Printer1284DeviceID</key>              \
	<string>1284deviceid</string>                                  \
	<key>dsAttrTypeStandard:PrinterLPRHost</key>                   \
	<string>rm</string>                                            \
	<key>dsAttrTypeStandard:PrinterLPRQueue</key>                  \
	<string>rp</string>                                            \
	<key>dsAttrTypeStandard:PrinterMakeAndModel</key>              \
	<string>makeandmodel</string>                                  \
	<key>dsAttrTypeStandard:PrinterType</key>                      \
	<string>ty</string>                                            \
	<key>dsAttrTypeStandard:PrinterURI</key>                       \
	<string>uri</string>                                           \
	<key>dsAttrTypeStandard:PrinterXRISupported</key>              \
	<string>xrisupported</string>                                  \
	<key>dsAttrTypeStandard:Protocols</key>                        \
	<string>protocols</string>                                     \
	<key>dsAttrTypeStandard:PwdAgingPolicy</key>                   \
	<string>PwdAgingPolicy</string>                                \
	<key>dsAttrTypeStandard:RARA</key>                             \
	<string>RARA</string>                                          \
	<key>dsAttrTypeStandard:RealName</key>                         \
	<string>realname</string>                                      \
	<key>dsAttrTypeStandard:RealUserID</key>                       \
	<string>ruid</string>                                          \
	<key>dsAttrTypeStandard:RecordName</key>                       \
	<string>name</string>                                          \
	<key>dsAttrTypeStandard:RecordType</key>                       \
	<string>dsAttrTypeStandard:RecordType</string>                 \
	<key>dsAttrTypeStandard:Relationships</key>                    \
	<string>relationships</string>                                 \
	<key>dsAttrTypeStandard:ResourceInfo</key>                     \
	<string>resourceinfo</string>                                  \
	<key>dsAttrTypeStandard:ResourceType</key>                     \
	<string>resourcetype</string>                                  \
	<key>dsAttrTypeStandard:SMBAccountFlags</key>                  \
	<string>smb_acctFlags</string>                                 \
	<key>dsAttrTypeStandard:SMBGroupRID</key>                      \
	<string>smb_group_rid</string>                                 \
	<key>dsAttrTypeStandard:SMBHome</key>                          \
	<string>smb_home</string>                                      \
	<key>dsAttrTypeStandard:SMBHomeDrive</key>                     \
	<string>smb_home_drive</string>                                \
	<key>dsAttrTypeStandard:SMBKickoffTime</key>                   \
	<string>smb_kickoff_time</string>                              \
	<key>dsAttrTypeStandard:SMBLogoffTime</key>                    \
	<string>smb_logoff_time</string>                               \
	<key>dsAttrTypeStandard:SMBLogonTime</key>                     \
	<string>smb_logon_time</string>                                \
	<key>dsAttrTypeStandard:SMBPasswordLastSet</key>               \
	<string>smb_pwd_last_set</string>                              \
	<key>dsAttrTypeStandard:SMBPrimaryGroupSID</key>               \
	<string>smb_primary_group_sid</string>                         \
	<key>dsAttrTypeStandard:SMBProfilePath</key>                   \
	<string>smb_profile_path</string>                              \
	<key>dsAttrTypeStandard:SMBRID</key>                           \
	<string>smb_rid</string>                                       \
	<key>dsAttrTypeStandard:SMBSID</key>                           \
	<string>smb_sid</string>                                       \
	<key>dsAttrTypeStandard:SMBScriptPath</key>                    \
	<string>smb_script_path</string>                               \
	<key>dsAttrTypeStandard:SMBUserWorkstations</key>              \
	<string>smb_user_workstations</string>                         \
	<key>dsAttrTypeStandard:ServiceType</key>                      \
	<string>servicetype</string>                                   \
	<key>dsAttrTypeStandard:SetupAssistantAdvertising</key>        \
	<string>spam</string>                                          \
	<key>dsAttrTypeStandard:SetupAssistantAutoRegister</key>       \
	<string>autoregister</string>                                  \
	<key>dsAttrTypeStandard:SetupAssistantLocation</key>           \
	<string>location</string>                                      \
	<key>dsAttrTypeStandard:State</key>                            \
	<string>state</string>                                         \
	<key>dsAttrTypeStandard:Street</key>                           \
	<string>street</string>                                        \
	<key>dsAttrTypeStandard:TimeToLive</key>                       \
	<string>timetolive</string>                                    \
	<key>dsAttrTypeStandard:URL</key>                              \
	<string>URL</string>                                           \
	<key>dsAttrTypeStandard:URLForNSL</key>                        \
	<string>URL</string>                                           \
	<key>dsAttrTypeStandard:UniqueID</key>                         \
	<string>uid</string>                                           \
	<key>dsAttrTypeStandard:UserShell</key>                        \
	<string>shell</string>                                         \
	<key>dsAttrTypeStandard:VFSDumpFreq</key>                      \
	<string>dump_freq</string>                                     \
	<key>dsAttrTypeStandard:VFSLinkDir</key>                       \
	<string>dir</string>                                           \
	<key>dsAttrTypeStandard:VFSOpts</key>                          \
	<string>opts</string>                                          \
	<key>dsAttrTypeStandard:VFSPassNo</key>                        \
	<string>passno</string>                                        \
	<key>dsAttrTypeStandard:VFSType</key>                          \
	<string>vfstype</string>                                       \
	<key>dsAttrTypeStandard:WeblogURI</key>                        \
	<string>webloguri</string>                                     \
	<key>dsAttrTypeStandard:XMLPlist</key>                         \
	<string>XMLPlist</string>                                      \
	<key>dsAttrTypeStandard:OrganizationInfo</key>                 \
	<string>organizationinfo</string>                              \
	<key>dsAttrTypeStandard:MapURI</key>                           \
	<string>mapURI</string>                                        \
	<key>dsAttrTypeStandard:CalendarPrincipalURI</key>             \
	<string>calendarprincipalURI</string>                          \
	<key>dsAttrTypeStandard:MapGUID</key>                          \
	<string>mapguid</string>                                       \
	<key>dsAttrTypeStandard:AuthorityRevocationList</key>			\
	<string>authorityrevocationlist</string>						\
	<key>dsAttrTypeStandard:CACertificate</key>						\
	<string>cacertificate</string>									\
	<key>dsAttrTypeStandard:CertificateRevocationList</key>			\
	<string>certificaterevocationlist</string>						\
	<key>dsAttrTypeStandard:CrossCertificatePair</key>				\
	<string>crosscertificatepair</string>							\
	<key>dsAttrTypeStandard:Owner</key>								\
	<string>owner</string>											\
	<key>dsAttrTypeStandard:UserCertificate</key>					\
	<string>usercertificate</string>								\
	<key>dsAttrTypeStandard:UserPKCS12Data</key>					\
	<string>userpkcs12data</string>									\
	<key>dsAttrTypeStandard:UserSMIMECertificate</key>				\
	<string>usersmimecertificate</string>							\
	<key>dsAttrTypeStandard:ProtocolNumber</key>					\
	<string>protocolnumber</string>									\
	<key>dsAttrTypeStandard:RPCNumber</key>							\
	<string>rpcnumber</string>										\
	<key>dsAttrTypeStandard:NetworkNumber</key>						\
	<string>networknumber</string>									\
	<key>dsAttrTypeStandard:AccessControlEntry</key>				\
	<string>accesscontrolentry</string>								\
	<key>dsAttrTypeStandard:AuthCredential</key>					\
	<string>authcredential</string>									\
	<key>dsAttrTypeStandard:KerberosRealm</key>						\
	<string>kerberosrealm</string>									\
	<key>dsAttrTypeStandard:NTDomainComputerAccount</key>			\
	<string>ntdomaincomputeraccount</string>						\
	<key>dsAttrTypeStandard:PrimaryNTDomain</key>					\
	<string>primaryntdomain</string>								\
	<key>dsAttrTypeStandard:TimePackage</key>						\
	<string>timepackage</string>									\
	<key>dsAttrTypeStandard:TotalSize</key>							\
	<string>totalsize</string>										\
	<key>dsAttrTypeStandard:NetGroupTriplet</key>					\
	<string>netgrouptriplet</string>								\
	<key>dsAttrTypeStandard:OriginalAuthenticationAuthority</key>	\
	<string>original_authentication_authority</string>				\
</dict>"

#define kDefaultLocalRecMappings	"<dict>                 \
	<key>dsRecTypeStandard:AFPServer</key>                  \
	<string>afpservers</string>                             \
	<key>dsRecTypeStandard:AFPUserAliases</key>             \
	<string>afpuser_aliases</string>                        \
	<key>dsRecTypeStandard:Aliases</key>                    \
	<string>aliases</string>                                \
	<key>dsRecTypeStandard:AppleMetaRecord</key>            \
	<string>dsRecTypeStandard:AppleMetaRecord</string>      \
	<key>dsRecTypeStandard:AutoServerSetup</key>            \
	<string>autoserversetup</string>                        \
	<key>dsRecTypeStandard:Bootp</key>                      \
	<string>bootp</string>                                  \
	<key>dsRecTypeStandard:ComputerLists</key>              \
	<string>computer_lists</string>                         \
	<key>dsRecTypeStandard:ComputerGroups</key>				\
	<string>computergroups</string>							\
	<key>dsRecTypeStandard:Computers</key>                  \
	<string>computers</string>                              \
	<key>dsRecTypeStandard:Config</key>                     \
	<string>config</string>                                 \
	<key>dsRecTypeStandard:Ethernets</key>                  \
	<string>ethernets</string>                              \
	<key>dsRecTypeStandard:FTPServer</key>                  \
	<string>ftpservers</string>                             \
	<key>dsRecTypeStandard:Groups</key>                     \
	<string>groups</string>                                 \
	<key>dsRecTypeStandard:HostServices</key>               \
	<string>hostservices</string>                           \
	<key>dsRecTypeStandard:Hosts</key>                      \
	<string>hosts</string>                                  \
	<key>dsRecTypeStandard:LDAPServer</key>                 \
	<string>ldapservers</string>                            \
	<key>dsRecTypeStandard:Locations</key>                  \
	<string>locations</string>                              \
	<key>dsRecTypeStandard:Machines</key>                   \
	<string>machines</string>                               \
	<key>dsRecTypeStandard:MetaUserNames</key>              \
	<string>dsRecTypeStandard:MetaUserNames</string>        \
	<key>dsRecTypeStandard:Mounts</key>                     \
	<string>mounts</string>                                 \
	<key>dsRecTypeStandard:NFS</key>                        \
	<string>mounts</string>                                 \
	<key>dsRecTypeStandard:Neighborhoods</key>              \
	<string>neighborhoods</string>                          \
	<key>dsRecTypeStandard:NetDomains</key>                 \
	<string>netdomains</string>                             \
	<key>dsRecTypeStandard:NetGroups</key>                  \
	<string>netgroups</string>                              \
	<key>dsRecTypeStandard:Networks</key>                   \
	<string>networks</string>                               \
	<key>dsRecTypeStandard:PasswordServer</key>             \
	<string>passwordservers</string>                        \
	<key>dsRecTypeStandard:People</key>                     \
	<string>people</string>                                 \
	<key>dsRecTypeStandard:PresetComputerLists</key>        \
	<string>presets_computer_lists</string>                 \
	<key>dsRecTypeStandard:PresetComputers</key>			\
	<string>presets_computers</string>						\
	<key>dsRecTypeStandard:PresetComputerGroups</key>		\
	<string>presets_computer_groups</string>				\
	<key>dsRecTypeStandard:PresetGroups</key>               \
	<string>presets_groups</string>                         \
	<key>dsRecTypeStandard:PresetUsers</key>                \
	<string>presets_users</string>                          \
	<key>dsRecTypeStandard:PrintService</key>               \
	<string>PrintService</string>                           \
	<key>dsRecTypeStandard:PrintServiceUser</key>           \
	<string>printserviceusers</string>                      \
	<key>dsRecTypeStandard:Printers</key>                   \
	<string>printers</string>                               \
	<key>dsRecTypeStandard:Protocols</key>                  \
	<string>protocols</string>                              \
	<key>dsRecTypeStandard:QTSServer</key>                  \
	<string>qtsservers</string>                             \
	<key>dsRecTypeStandard:RPC</key>                        \
	<string>rpcs</string>                                   \
	<key>dsRecTypeStandard:Resources</key>                  \
	<string>resources</string>                              \
	<key>dsRecTypeStandard:SMBServer</key>                  \
	<string>smbservers</string>                             \
	<key>dsRecTypeStandard:Server</key>                     \
	<string>servers</string>                                \
	<key>dsRecTypeStandard:Services</key>                   \
	<string>services</string>                               \
	<key>dsRecTypeStandard:SharePoints</key>                \
	<string>config/SharePoints</string>                     \
	<key>dsRecTypeStandard:Users</key>                      \
	<string>users</string>                                  \
	<key>dsRecTypeStandard:WebServer</key>                  \
	<string>httpservers</string>                            \
	<key>dsRecTypeStandard:Augments</key>					\
	<string>augments</string>								\
</dict>"


// node dict keys
#define kNodeFilePathkey			"Node File Path"
#define kNodePathkey				"Node Path"
#define kNodeNamekey				"Node Name"
#define kNodeObjectkey				"Node Object"
#define kNodeUIDKey					"Node UID"
#define kNodeEffectiveUIDKey		"Node Effective UID"
#define kNodeAuthenticatedUserName	"Node Authenticated User Name"
#define kNodePWSDirRef				"Node PWS Dir Ref"
#define kNodePWSNodeRef				"Node PWS Node Ref"
#define kNodeLDAPDirRef				"Node LDAP Dir Ref"
#define kNodeLDAPNodeRef			"Node LDAP Node Ref"

// dsDoDirNodeAuth continue data keys
#define kAuthContinueDataHandlerTag				"Handler Tag"
#define kAuthContinueDataAuthAuthority			"Auth Authority"
#define kAuthContinueDataMutableRecordDict		"Record Dict"
#define kAuthContinueDataAuthedUserIsAdmin		"Authed User Is Admin"
#define kAuthCOntinueDataPassPluginContData		"Password Plugin Continue Data"

//auth type tags
#define kHashNameListPrefix				"HASHLIST:"
#define kHashNameNT						"SMB-NT"
#define kHashNameLM						"SMB-LAN-MANAGER"
#define kHashNameCRAM_MD5				"CRAM-MD5"
#define kHashNameSHA1					"SALTED-SHA1"
#define kHashNameRecoverable			"RECOVERABLE"
#define kHashNameSecure					"SECURE"

#define kDBPath				"/var/db/dslocal/"
#define kDBNodesPath		"/var/db/dslocal/nodes"
#define kNodesDir			"nodes"
#define kDictionaryType		"Dict Type"

// continue data keys
#define kContinueDataRecordsArrayKey			"Records Array"
#define kContinueDataNumRecordsReturnedKey		"Num Records Returned"
#define kContinueDataDesiredAttributesArrayKey	"Desired Attributes Array"

// node info dict for use in GetDirNodeInfo
#define kNodeInfoDictType						"Node Info"
#define kNodeInfoAttributes						"Attributes"

// attrValueListRef dict
#define kAttrValueListRefAttrName				"Attribute"

// open record dict
#define kOpenRecordDictRecordFile				"Record File"
#define kOpenRecordDictRecordType				"Record Type"
#define kOpenRecordDictAttrsValues				"Attributes Values"
#define kOpenRecordDictNodeDict					"Node Dict"
#define kOpenRecordDictIsDeleted				"Is Deleted"

#define kDefaultNodeName		"Default"
#define kTargetNodeName			"Target"
#define kTopLevelNodeName		"Local"
#define kLocalNodePrefix		"/Local"
#define kLocalNodePrefixRoot	"/Local/"
#define kLocalNodeDefault		"/Local/Default"
#define kLocalNodeDefaultRoot	"/Local/Default/"

enum {
	ePluginHashLM							= 0x0001,
	ePluginHashNT							= 0x0002,
	ePluginHashSHA1							= 0x0004,		/* deprecated */
	ePluginHashCRAM_MD5						= 0x0008,
	ePluginHashSaltedSHA1					= 0x0010,
	ePluginHashRecoverable					= 0x0020,
	ePluginHashSecurityTeamFavorite			= 0x0040,
	
	ePluginHashDefaultSet			= ePluginHashSaltedSHA1,
	ePluginHashWindowsSet			= ePluginHashNT | ePluginHashSaltedSHA1 | ePluginHashLM,
	ePluginHashDefaultServerSet		= ePluginHashNT | ePluginHashSaltedSHA1 | ePluginHashLM |
										ePluginHashCRAM_MD5 | ePluginHashRecoverable,
	ePluginHashAll					= 0x007F,
	ePluginHashHasReadConfig		= 0x8000
};

#define kCustomCallFlushRecordCache			FOUR_CHAR_CODE( 'FCCH' )

class CDSLocalPluginNode;

class CDSLocalPlugin : public BaseDirectoryPlugin
{
	public:
		bool					mPWSFrameworkAvailable;
	
								CDSLocalPlugin( FourCharCode inSig, const char *inName );
		virtual					~CDSLocalPlugin( void );

		// required pure virtuals for BaseDirectoryPlugin
		virtual SInt32			Initialize				( void );
		virtual SInt32			SetPluginState			( const UInt32 inState );
		virtual SInt32			PeriodicTask			( void );
		virtual CDSAuthParams*	NewAuthParamObject		( void );
	
		static SInt32			FillBuffer				( CFMutableArrayRef inRecordList, BDPIOpaqueBuffer inData );
		static const char		*GetCStringFromCFString	( CFStringRef inCFString, char **outCString );
		static void				FilterAttributes		( CFMutableDictionaryRef inRecord, CFArrayRef inRequestedAttribs, CFStringRef inNodeName );
		
		void					CloseDatabases( void );
	
		// from CDSServerModule:
		virtual SInt32			Validate( const char *inVersionStr, const UInt32 inSignature );
		virtual SInt32			ProcessRequest( void *inData );

		CFStringRef				AttrNativeTypeForStandardType( CFStringRef inStdType );
		CFStringRef				AttrStandardTypeForNativeType( CFStringRef inNativeType );
		CFStringRef				AttrPrefixedNativeTypeForNativeType( CFStringRef inNativeType );
		CFStringRef				RecordNativeTypeForStandardType( CFStringRef inStdType );
		CFStringRef				RecordStandardTypeForNativeType( CFStringRef inNativeType );
		CFStringRef				RecordPrefixedNativeTypeForNativeType( CFStringRef inNativeType );

		CFStringRef				GetUserGUID( CFStringRef inUserName, CDSLocalPluginNode* inNode );
		
		bool					UserIsAdmin( CFStringRef inUserName, CDSLocalPluginNode* inNode );
		bool					UserIsAdmin( CFDictionaryRef inRecordDict, CDSLocalPluginNode* inNode );
		bool					SearchAllRecordTypes( CFArrayRef inRecTypesArray );
		
		// internal wrappers for task handlers
		tDirStatus				CreateRecord( tDirNodeReference inNodeRef, CFStringRef inStdRecType, CFStringRef inRecName,
									bool inOpenRecord, tRecordReference* outOpenRecordRef = NULL );
		tDirStatus				OpenRecord( tDirNodeReference inNodeRef, CFStringRef inStdRecType, CFStringRef inRecName,
									tRecordReference* outOpenRecordRef );
		tDirStatus				CloseRecord( tRecordReference inRecordRef );
		tDirStatus				AddAttribute( tRecordReference inRecordRef, CFStringRef inAttributeName,
									CFStringRef inAttrValue );
		tDirStatus				RemoveAttribute( tRecordReference inRecordRef, CFStringRef inAttributeName );		
		tDirStatus				GetRecAttrValueByIndex( tRecordReference inRecordRef, const char *inAttributeName,
									UInt32 inIndex, tAttributeValueEntryPtr *outEntryPtr );	
		tDirStatus				AddAttributeValue( tRecordReference inRecordRef, const char *inAttributeType,
									const char *inAttributeValue );
		tDirStatus				GetRecAttribInfo( tRecordReference inRecordRef, const char *inAttributeType,
									tAttributeEntryPtr *outAttributeInfo );
		
		// auth stuff
		tDirStatus				AuthOpen( tDirNodeReference inNodeRef, const char* inUserName, const char* inPassword,
									bool inUserIsAdmin, bool inIsEffectiveRoot=false );
		UInt32					DelayFailedLocalAuthReturnsDeltaInSeconds()
									{ return mDelayFailedLocalAuthReturnsDeltaInSeconds; }

		void					AddContinueData( tDirNodeReference inNodeRef, CFMutableDictionaryRef inContinueData, tContextData *inOutAttachReference );
		CFDictionaryRef			RecordDictForRecordRef( tRecordReference inRecordRef );
		CFMutableDictionaryRef	CopyNodeDictForNodeRef( tDirNodeReference inNodeRef );
		CDSLocalPluginNode*		NodeObjectFromNodeDict( CFDictionaryRef inNodeDict );
		CDSLocalPluginNode*		NodeObjectForNodeRef( tDirNodeReference inNodeRef );
		CFArrayRef				CreateOpenRecordsOfTypeArray( CFStringRef inNativeRecType );

		static void					ContextDeallocProc( void* inContextData );
		static const char*			CreateCStrFromCFString( CFStringRef inCFStr, char **ioCStr );
		static CFMutableArrayRef	CFArrayCreateFromDataList( tDataListPtr inDataList );
		static void					LogCFDictionary( CFDictionaryRef inDict, CFStringRef inPrefixMessage );
		static tDirStatus			OpenDirNodeFromPath( CFStringRef inPath, tDirReference inDSRef,
										tDirNodeReference* outNodeRef );
		static	void				WakeUpRequests( void );

		tDirStatus					GetDirServiceRef( tDirReference *outDSRef );
	
		CFTypeRef					NodeDictCopyValue( CFDictionaryRef inNodeDict, const void *inKey );
		void						NodeDictSetValue( CFMutableDictionaryRef inNodeDict, const void *inKey, const void *inValue );
		
		CFMutableDictionaryRef		mOpenRecordRefs;

	protected:
	
		// required pure virtuals for BaseDirectoryPlugin
		virtual CFDataRef			CopyConfiguration		( void );
		virtual bool				NewConfiguration		( const char *inData, UInt32 inLength );
		virtual bool				CheckConfiguration		( const char *inData, UInt32 inLength );
		virtual tDirStatus			HandleCustomCall		( sBDPINodeContext *pContext, sDoPlugInCustomCall *inData );
		virtual bool				IsConfigureNodeName		( CFStringRef inNodeName );
		virtual BDPIVirtualNode		*CreateNodeForPath		( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID );

		// subclass
		void						WaitForInit( void );

	private:

		// task handlers
		virtual tDirStatus		OpenDirNode( sOpenDirNode* inData );
		virtual tDirStatus		CloseDirNode( sCloseDirNode* inData );
		virtual tDirStatus		GetDirNodeInfo( sGetDirNodeInfo* inData );
		virtual tDirStatus		GetRecordList( sGetRecordList* inData );
		virtual tDirStatus		DoAttributeValueSearch( sDoAttrValueSearch* inData );
		virtual tDirStatus		DoAttributeValueSearchWithData( sDoAttrValueSearchWithData* inData );
		virtual tDirStatus		DoMultipleAttributeValueSearch( sDoMultiAttrValueSearch* inData );
		virtual tDirStatus		DoMultipleAttributeValueSearchWithData( sDoMultiAttrValueSearchWithData* inData );
		virtual tDirStatus		ReleaseContinueData( sReleaseContinueData* inData );
		virtual tDirStatus		GetAttributeEntry( sGetAttributeEntry* inData );
		virtual tDirStatus		GetAttributeValue( sGetAttributeValue* inData );
		virtual tDirStatus		CloseAttributeValueList( sCloseAttributeValueList* inData );
		virtual tDirStatus		OpenRecord( sOpenRecord* inData );
		virtual tDirStatus		CloseRecord( sCloseRecord* inData );

	public:
		virtual tDirStatus		FlushRecord( sFlushRecord* inData );
		virtual tDirStatus		SetAttributeValues( sSetAttributeValues* inData, const char *inRecTypeStr );

	private:
		virtual tDirStatus		SetRecordName( sSetRecordName* inData );
		virtual tDirStatus		DeleteRecord( sDeleteRecord* inData );
		virtual tDirStatus		CreateRecord( sCreateRecord* inData );
		virtual tDirStatus		AddAttribute( sAddAttribute* inData, const char *inRecTypeStr );
		virtual tDirStatus		AddAttributeValue( sAddAttributeValue* inData, const char *inRecTypeStr );
		virtual tDirStatus		RemoveAttribute( sRemoveAttribute* inData, const char *inRecTypeStr );
		virtual tDirStatus		RemoveAttributeValue( sRemoveAttributeValue* inData, const char *inRecTypeStr );
		virtual tDirStatus		SetAttributeValue( sSetAttributeValue* inData, const char *inRecTypeStr );
		virtual tDirStatus		GetRecAttrValueByID( sGetRecordAttributeValueByID* inData );
		virtual tDirStatus		GetRecAttrValueByIndex( sGetRecordAttributeValueByIndex* inData );
		virtual tDirStatus		GetRecAttrValueByValue( sGetRecordAttributeValueByValue* inData );
		virtual tDirStatus		GetRecRefInfo( sGetRecRefInfo* inData );
		virtual tDirStatus		GetRecAttribInfo( sGetRecAttribInfo* inData );
		virtual tDirStatus		DoAuthentication( sDoDirNodeAuth *inData, const char *inRecTypeStr,
													CDSAuthParams &inParams );
		
		bool					BufferFirstTwoItemsEmpty( tDataBufferPtr inAuthBuffer );
		void					AuthenticateRoot( CFMutableDictionaryRef nodeDict, bool *inAuthedUserIsAdmin, CFStringRef *inOutAuthedUserName );
		virtual tDirStatus		DoPlugInCustomCall( sDoPlugInCustomCall* inData );
		virtual tDirStatus		HandleNetworkTransition( sHeader* inData );

		// internal-use methods
		void					FreeExternalNodesInDict( CFMutableDictionaryRef inNodeDict );
		bool					CreateLocalDefaultNodeDirectory( void );
		CFMutableArrayRef		FindSubDirsInDirectory( const char* inDir );
		const char*				GetProtocolPrefixString();
		bool					LoadMappings();
		void					LoadSettings();

		tDirStatus				PackRecordsIntoBuffer( unsigned long inFirstRecordToReturnIndex,
									CFArrayRef inRecordsArray, CFArrayRef inDesiredAttributes, tDataBufferPtr inBuff,
									bool inAttrInfoOnly, unsigned long* outNumRecordsPacked );
		void					AddRecordTypeToRecords( CFStringRef inStdType, CFArrayRef inRecordsArray );
		tDirStatus				AddMetanodeToRecords( CFArrayRef inRecords, CFStringRef inNode );
		void					RemoveUndesiredAttributes( CFArrayRef inRecords, CFArrayRef inDesiredAttributes );
		tDirStatus				ReadHashConfig( CDSLocalPluginNode* inNode );

		CFDictionaryRef			CreateDictionariesFromFiles( CFArrayRef inFilePaths );

								// may throw eMemoryAllocError
		CFDictionaryRef			CreateNodeInfoDict( CFArrayRef inDesiredAttrs, bool attrInfoOnly, CFDictionaryRef inNodeDict );

		tDirStatus				GetRetainedRecordDict( CFStringRef inRecordName, CFStringRef inNativeRecType,
									CDSLocalPluginNode* inNode, CFMutableDictionaryRef* outRecordDict );
		bool					RecurseUserIsMemberOfGroup( CFStringRef inUserRecordName, CFStringRef inUserGUID,
									CFStringRef inUserGID, CFDictionaryRef inGroupDict, CDSLocalPluginNode* inNode );

		void					LockOpenAttrValueListRefs( bool inWriting );
		void					UnlockOpenAttrValueListRefs();
		CFTypeRef				GetAttrValueFromInput( const char* inData, UInt32 inDataLength );
		CFMutableArrayRef		CreateCFArrayFromGenericDataList( tDataListPtr inDataList );
		CFDictionaryRef			CopyNodeDictandNodeObject( CFDictionaryRef inOpenRecordDict, CDSLocalPluginNode **outPluginNode );
		static void				ContinueDeallocProc( void *inContinueData );
		
		static void				HandleFlushCaches( CFRunLoopTimerRef timer, void *inInfo );
		void					FlushCaches( CFStringRef inStdType );

	private:
		DSMutexSemaphore			mOpenNodeRefsLock;
		DSMutexSemaphore			mOpenRecordRefsLock;
		DSMutexSemaphore			mOpenAttrValueListRefsLock;
		DSMutexSemaphore			mGeneralPurposeLock;

		UInt32						mSignature;
        CFMutableDictionaryRef		mOpenNodeRefs;
		CFMutableDictionaryRef		mOpenAttrValueListRefs;
		UInt32						mHashList;
		UInt32						mDelayFailedLocalAuthReturnsDeltaInSeconds;

		CFDictionaryRef				mAttrNativeToStdMappings;
		CFDictionaryRef				mAttrStdToNativeMappings;
		CFMutableDictionaryRef		mAttrPrefixedNativeToNativeMappings;
		CFMutableDictionaryRef		mAttrNativeToPrefixedNativeMappings;
		CFDictionaryRef				mRecNativeToStdMappings;
		CFDictionaryRef				mRecStdToNativeMappings;
		CFMutableDictionaryRef		mRecPrefixedNativeToNativeMappings;
		tDirReference				mDSRef;
};

#endif
