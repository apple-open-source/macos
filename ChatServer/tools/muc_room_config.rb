#!/usr/bin/ruby -w
# == Synopsis
#
# This tool will create, configure, and destroy Jabber multi-user chat rooms (XEP-0045).  It is designed to work with
# Apple's version of mu-conference 0.62.  It needs to login to the Jabber server as a client, so valid authentication 
# credentials are required. A custom XMPP4R library must be installed which works around some bugs in the present XMPP4R trunk.
#
# == Usage
# muc_room_config.rb [options] <command> <command options>
#   Environment:
#     Authentication credentials (username and password) must be passed in using these environment variables.  These are
#     REQUIRED:
#       MUC_USER : Username for Jabber login
#       MUC_PASS : Password for Jabber login
#    
#   Options:
#     -D, --debug: Enable debug mode
#     -o, --hostname:  Jabber server hostname (REQUIRED)
#     -r, --roomname:  Name of Jabber MUC room to create/modify/use (REQUIRED)
#     -R, --reason:  Reason, a message passed to users during room invitation or room destroy
#     -e, --resource:  Specify a custom resource for the tool's Jabber ID
#
#   Commands:
#     Choose one command per execution:
#     -c, --create:  Create and configure a MUC room.
#       usage:  -c
#     -i, --invite:  Sends chat room invitations to 1 or more JIDs. The room must
#       already be created and configured. 
#       usage:  -i <JID> (<JID> <JID> ...)
#     -d, --destroy:  Destroy a room (not yet implemented)
#     -h, -?, --help: This usage information
#     --default: Display the default room configuration and exit
#
#   Additional room configuration options:
#     Use the following arguments to specify non-default options for room configuration (for use with -c command):   
#       Room Description:
#         --roomdesc <string>
#       Leave message (displayed when a user exits the room):
#         --leave <string>
#       Join message (displayed when a user joins the room):
#         --join <string>
#       Message for user renaming nickname in room:
#         --rename <string>
#       Allow Occupants to Change Subject:
#         --changesubject <true | false>
#       Maximum Number of Room Occupants (0 for unlimited):
#         --maxusers <number>
#       Allow Occupants to query other Occupants:
#         --privacy <true | false>
#       Allow Public Searching for Room:
#         --publicroom <true | false>
#       Make Room Persistent:
#         --persistentroom <true | false>
#       Consider all Clients as Legacy (shown messages):
#         --legacy <true | false>
#       Make Room Moderated (By default, new users entering a moderated room are only visitors):
#         --moderatedroom <true | false>
#       Make Occupants in a Moderated Room Default to Participant:
#         --defaulttype <true | false>
#       Ban Private Messages between Occupants:
#         --privmsg <true | false>
#       An Invitation is Required to Enter:
#         --inviteonly <true | false>
#       Allow Occupants to Invite Others:
#         --allowinvites <true | false>
#       A Password is required to enter?
#         --passwordprotected <true | false>
#       The Room Password:
#         --roomsecret <string>
#       Affiliations that May Discover Real JIDs of Occupants:
#         --whois <anyone | moderators | none>
#       Enable Logging of Room Conversations:
#         --enablelogging <true | false>
#       Logfile format:
#         --logformat <text | xml | xhtml> 
#
#   
#   Examples:
#      env MUC_USER="admin" MUC_PASS="let_me_in" ruby muc_room_config.rb -o chatserver.example.com -r my_room -c --enablelogging true --logformat text --join "Welcome to my_room, enjoy your stay"
#      env MUC_USER="admin" MUC_PASS="let_me_in" ruby muc_room_config.rb -o chatserver.example.com -r my_room -i bob@chatserver.example.com alice@chatserver.example.com
#      env MUC_USER="admin" MUC_PASS="let_me_in" ./muc_room_config.rb -o chatserver.example.com -r my_room -d
#
# == Author
# Joel Hedden (jhedden@apple.com)
#
# == Copyright
# Copyright 2007-2009 Apple Inc. All rights reserved.
#
# == State
# Functional, unfinished
#
# == Todo
# * error handling
# * list/grant/revoke room ownership
# * add logging mechanism
#

require 'xmpp4r'
require 'xmpp4r/muc'
#require 'xmpp4r/muc/x'
require 'rdoc/usage'
include Jabber
include MUC


#################
# MucRoomTool Class

class MucRoomTool

  # Define constants
  TOOL_VERSION = "0.2"
  DEFAULT_RESOURCE = 'RubyXMPP4R'
  DEFAULT_REASON_DESTROY = "Your workgroup has been deleted."
  DEFAULT_REASON_INVITE = "You have been automatically invited to this chatroom for an OD workgroup."
  BOOL = { 'true' => 1, 'false' => 0 }

  def initialize
    # Set default room configuration
    @room_config = {
      'FORM_TYPE' => 'http://jabber.org/protocol/muc#roomconfig',
      'form' => 'config',
      'muc#roomconfig_roomname' => nil,
      'muc#roomconfig_roomdesc' => nil,
      'leave' => 'has left',
      'join' => 'has become available',
      'rename' => 'is now known as',
      'muc#roomconfig_changesubject' => 0,
      'muc#roomconfig_maxusers' => 0,
      'privacy' => 0,
      'muc#roomconfig_publicroom' => 1,
      'muc#roomconfig_persistentroom' => 1,
      'legacy' => 0,
      'muc#roomconfig_moderatedroom' => 0,
      'defaulttype' => 0,
      'privmsg' => 0,
      'muc#roomconfig_membersonly' => 0,
      'muc#roomconfig_allowinvites' => 0,
      'muc#roomconfig_passwordprotectedroom' => 0,
      'muc#roomconfig_roomsecret' => nil,
      'muc#roomconfig_whois' => 'moderators',
      'muc#roomconfig_enablelogging' => 0,
      'logformat' => 'xml'
    }

    # Initialize instance variables
    @debug = false
    @username = nil
    @password = nil
    @roomname = nil
    @hostname = nil
    @client = nil
    @invite_list = nil
    @invite = false
    @create = false
    @destroy = false
    @reason = nil
    @alt_roomname = nil
    @room_jid = nil
    @client_jid = nil
    @client_full_jid = nil
    @client_muc_jid = nil
    @resource = nil

  end


  def parse_options
    require 'getoptlong'

    opts = GetoptLong.new(
      # Commands
      [ '--help', '-h', '-?', GetoptLong::NO_ARGUMENT ],
      [ '--default', GetoptLong::NO_ARGUMENT ],
      [ '--create', '-c', GetoptLong::NO_ARGUMENT ],
      [ '--invite', '-i', GetoptLong::NO_ARGUMENT ],
      [ '--destroy', '-d', GetoptLong::NO_ARGUMENT ],

      # Options
      [ '--debug', '-D', GetoptLong::NO_ARGUMENT ],
      [ '--hostname', '-o', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--roomname', '-r', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--reason', '-R', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--resource', '-e', GetoptLong::REQUIRED_ARGUMENT ],

      #  Optional args for room configuration:
      [ '--roomdesc', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--leave',  GetoptLong::REQUIRED_ARGUMENT ],
      [ '--join', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--rename', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--changesubject', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--maxusers', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--privacy', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--publicroom', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--persistentroom', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--legacy', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--moderatedroom', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--defaulttype', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--privmsg', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--inviteonly', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--allowinvites', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--passwordprotectedroom', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--roomsecret', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--whois', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--enablelogging', GetoptLong::REQUIRED_ARGUMENT ],
      [ '--logformat', GetoptLong::REQUIRED_ARGUMENT ]
    )

    opts.each do |opt, arg|
      case opt
        # Commands
        when '--help'
          RDoc::usage('usage')
        when '--default'
          display_default_room_config
          exit 0
        when '--create'
          @create = true
        when '--invite'
          @invite = true
        when '--destroy'
          @destroy = true

        # Options
        when '--debug'
          @debug = true
          Jabber::debug = true
        when '--hostname'
          @hostname = arg
        when '--roomname'
          @roomname = arg
          @room_config['muc#roomconfig_roomname'] = arg
        when '--reason'
          @reason = arg
        when '--resource'
          @resource = arg

        #  Optional args for room configuration:
        when '--roomdesc'
          @room_config['muc#roomconfig_roomdesc'] = arg
        when '--leave'
          @room_config['leave'] = arg
        when '--join'
          @room_config['join'] = arg
        when '--rename'
          @room_config['rename'] = arg
        when '--changesubject'
          @room_config['muc#roomconfig_changesubject'] = BOOL[arg]
        when '--maxusers'
          @room_config['muc#roomconfig_maxusers'] = arg
        when '--privacy'
          @room_config['privacy'] = BOOL[arg]
        when '--publicroom'
          @room_config['muc#roomconfig_publicroom'] = BOOL[arg]
        when '--persistentroom'
          @room_config['muc#roomconfig_persistentroom'] = BOOL[arg]
        when '--legacy'
          @room_config['legacy'] = BOOL[arg]
        when '--moderatedroom'
          @room_config['muc#roomconfig_moderatedroom'] = BOOL[arg]
        when '--defaulttype'
          @room_config['defaulttype'] = BOOL[arg]
        when '--privmsg'
          @room_config['privmsg'] = BOOL[arg]
        when '--inviteonly'
          @room_config['muc#roomconfig_membersonly'] = BOOL[arg]
        when '--allowinvites'
          @room_config['muc#roomconfig_allowinvites'] = BOOL[arg]
        when '--passwordprotectedroom'
          @room_config['muc#roomconfig_passwordprotectedroom'] = BOOL[arg]
        when '--roomsecret'
          @room_config['muc#roomconfig_roomsecret'] = arg
        when '--whois'
          @room_config['muc#roomconfig_whois'] = arg
        when '--enablelogging'
          @room_config['muc#roomconfig_enablelogging'] = BOOL[arg]
        when '--logformat'
          @room_config['logformat'] = arg
      end
    end

    # JJJ debugging: @room_config.each {|key, value| puts "#{key} is #{value}"}

    #  Assign defaults where needed
    if (@resource == nil)
      @resource = DEFAULT_RESOURCE
    end
    if (@reason == nil)
      if (@invite)
        @reason = DEFAULT_REASON_INVITE
      elsif (@destroy)
        @reason = DEFAULT_REASON_DESTROY
      end
    end
    
    # if --invite was specified, load the remaining args into a JID array
    if (@invite) 
      @invite_list = ARGV
    end

    # Get authentication args from environment
    @username = ENV['MUC_USER']
    @password = ENV['MUC_PASS']

    # Configure variables that are based on user input
    if (@username && @password && @hostname && @resource && @roomname)
      @room_jid = "#{@roomname}@conference.#{@hostname}"
      @client_jid = "#{@username}@#{@hostname}"
      @client_full_jid = "#{@client_jid}/#{@resource}"
      @client_muc_jid = "#{@room_jid}/#{@username}"
      @room_config['muc#owner_roomname'] = @roomname
      if (! @room_config['muc#owner_roomdesc'])
        @room_config['muc#owner_roomdesc'] = @roomname
      end
      @alt_roomname = "#{@roomname}_alt@conference.#{@hostname}"
    else
      puts "Some required arguments were not provided. See usage:\n"
      RDoc::usage_no_exit('usage')
      exit 1
    end

  end


  # This is rewritten from MUC::MUCClient::Configure to work around a couple of bugs:
  # 1. original code calls stream.send_with_id(iq) which returns NIL
  # 2. original code sends the wrong JID when requesting a config form, so it failed
  def configure_room(client, muc, room_jid, options={})
    raise 'You are not the owner' unless muc.owner?

    iq = Iq.new(:get, room_jid)
    iq.to = room_jid
    iq.from = muc.my_jid
    iq.add(IqQueryMUCOwner.new)

    fields = []

    #answer = client.send_with_id(iq)
    #raise "Configuration not possible for this room" unless answer.query && answer.query.x(XData)

        #answer.query.x(XData).fields.each { |field|
        #  if (var = field.attributes['var'])
        #    fields << var
        #  end
        #}
    #JJJ send_with_id was returning NIL (when an answer was available) so the above failed.
    # The following may not raise an exception if we get no answer.
    client.send_with_id(iq)

    # fill out the reply form
    iq = Iq.new(:set, room_jid)
    iq.to = room_jid
    iq.from = muc.my_jid
    query = IqQueryMUCOwner.new
    form = Dataforms::XData.new
    form.type = :submit
    options.each do |var, values|
      field = Dataforms::XDataField.new
      values = [values] unless values.is_a?(Array)
      field.var, field.values = var, values
      form.add(field)
    end
    query.add(form)
    iq.add(query)

    client.send_with_id(iq)
    return true
  end


  def destroy_room(client, client_full_jid, room_jid, reason, alt_room_jid)
    iq = Iq.new(:set, room_jid)
    iq.to = room_jid
    iq.from = client_full_jid
    query = IqQueryMUCOwner.new
    elem_destroy = REXML::Element::new('destroy')
    if (alt_room_jid)
      elem_destroy.attributes['jid'] = alt_room_jid
    end
    if (reason)
      elem_reason = REXML::Element::new('reason').add_text(reason)
      elem_destroy.add_element(elem_reason)
    end
    query.add(elem_destroy)
    iq.add(query)
    client.send_with_id(iq)
    return true
  end


  def join_room(client, client_muc_jid, client_full_jid)
    muc = MUCClient.new(client)
    muc.my_jid = client_full_jid
    muc.join(client_muc_jid)
    muc
  end


  def connect_client(client_full_jid, password)
    client = Client.new(client_full_jid)
    client.connect
    client.auth_nonsasl(password, false)
    client.send(Presence::new)
    client
  end


  def send_invites(client, client_jid, room_jid, reason, recipients)
    recipients.each { |jid|
      msg = Message.new
      msg.from = client_jid
      msg.to = room_jid
      x = msg.add(XMUCUser.new)
      x.add(XMUCUserInvite.new(jid, reason))
      client.send(msg)
    }
    return true
  end


  def display_default_room_config
    puts "Default room configuration (room name and description are configured by the -r argument):\n"
    @room_config.each {|key, value| puts "#{key} = #{value}"}
  end


#### Unfinished
def room_exists?()
end 

    
#   
def get_room_owners()
end     
        
#       
def grant_room_ownership()
end     
    
#   
def revoke_room_ownership()
end
####


########################################################################
# Execute: Perform operations based on user input options
  def execute

    client = connect_client(@client_full_jid, @password)

    ### Perform the user-requested operation(s)
    # Create a room
    if (@create && @roomname)
      puts "Creating room #{@roomname}...\n"
      #if room_exists?(@roomname)
      muc = join_room(client, @client_muc_jid, @client_full_jid)
      configure_room(client, muc, @room_jid, @room_config)
      puts "Done creating and configuring room #{@roomname}.\n"
    end

    # Invite users to a room (room must exist!)
    if (@invite && @roomname && @invite_list)
      puts "Joining room #{@roomname}...\n"
      muc = join_room(client, @client_muc_jid, @client_full_jid)
      puts "Sending chat room invitations...\n" 
      send_invites(client, @client_full_jid, @room_jid, @reason, @invite_list)
      puts "Done sending chat room invitations.\n" 
    end

    # Destroy a room
    if (@destroy && @roomname)
      puts "Destroying room #{@roomname}...\n"
      join_room(client, @client_muc_jid, @client_full_jid)
      destroy_room(client, @client_full_jid, @room_jid, @reason, @alt_roomname)
      puts "Done destroying room #{@roomname}...\n"
    end

    # Apparently if we disconnect before c2s has routed our final packet(s), those packets get dropped by c2s.
    sleep(1)

    return true
  end

end  # class MucRoomTool

# JJJ: get completed room config form (create a method...)
#iq = Iq.new(:get, $room_jid)
#iq.from = $client_full_jid
#query = IqQueryMUCOwner.new
#iq.add(query)
#$client.send_with_id(iq)



#################
# MAIN
muc_room_tool = MucRoomTool.new
muc_room_tool.parse_options
status = muc_room_tool.execute ? 0 : 1
exit status
