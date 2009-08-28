#!/usr/bin/ruby -rubygems
# This script reads in a line-delimited list of jabberd 1.x XML spool files (1 filename per line) and migrates
# the data in each spool file to a SQLite database file for use with Jabberd 2.1
# The SQLite database file must already exist with the standard jaberd 2.1 schema (as created by db-setup.sqlite)
#
# Copyright 2009 Apple Computer Inc. All rights reserved.
#

require "rexml/document"
require "getoptlong"
require "sqlite3"

def usage()
  puts "\nusage: #{File.basename($0)} [opts] <input spool directory> <output file>\n"
  puts "Arguments:"
  puts "-h, --help\tDisplay usage info"
  puts "-d, --debug\tEnable debug mode"
  puts "-r, --realm\tSpecify a realm to use for JIDs.  Otherwise it will be taken from the input spool file path."
  puts ""
  puts "<input spool directory>: Path of the jabberd 1.4 spool file directory, eg. /var/jabber/spool/hostname.domain"
  puts ""
  puts "<output file>: Path of destination SQLite database file to be used as the data destination."
  puts "\tThe file needs to have the same schema as used by the jabberd 2.x db-setup.sqlite script."
  puts ""
end

def log_message(str)
  logfile = File.new(@options['logpath'], 'a')
  logstr = "#{Time.now}: #{File.basename($0)}: #{str}"
  if @options['debug']
    puts logstr
  end
  logfile.puts logstr
  logfile.close
end

def bail(str)
  log_message(str)
  exit 1
end

def quote(str)
  SQLite3::Database.quote(str)
end

# return true if we fail but want to proceed to the next file
# return false if we fail and want to abort/db rollback
def migrate_file(file)
  if @options['realm'] == "" then
    if file =~ /^.*\/(.+)\/(.+).xml$/ then
      realm = $1
      username = $2
    else
      log_message("ERROR: Cannot get realm and/or username from paths provided, for line:\n\t#{file}")
      return true
    end
  else
    if file =~ /^.*\/(.+).xml$/ then
      realm = @options['realm']
      username = $1
    else
      log_message("ERROR: Cannot get username from paths provided, for line: #{file}")
      return true
    end
  end  
  jid = username +  "@" + realm
  
  log_message("Processing file for user: #{jid}...")

  if ! File.exist?(file) then
    log_message("ERROR: File does not exist: #{file}")
    return true
  end
  spoolfile = File.new(file, 'r')

  begin
    doc = REXML::Document.new(spoolfile)
  rescue REXML::ParseException => ex
    log_message("ERROR: Cannot parse file: #{ex.message}")
    return true
  end

  query = "INSERT INTO active (`collection-owner`) VALUES ( ? );"
  prep = $db.prepare(query)
  prep.bind_params(jid)
  log_message("Prepared query: #{query}\n\tval: #{jid}")
  prep.execute
  prep.close

  #puts db.prepare("INSERT INTO authreg (username, realm) VALUES ('#{username}', '#{realm}');")
  doc.root.elements.each do |element|
    qname = element.name
    if element.namespace != nil then
      qname = element.namespace + " " + qname
    end

    case qname
    #when "jabber:iq:auth:0k zerok"
      #puts "UPDATE authreg SET hash = '#{element.elements["hash"].text}', token = '#{element.elements["token"].text}', " +
        #"sequence = #{element.elements["sequence"].text} WHERE username = '#{username}' AND realm = '#{realm}';"

    #when "jabber:iq:auth password"
      #puts "UPDATE authreg SET `password` = '#{element.text.nil? ? '' : element.text}' " +
        #{}"WHERE username = '#{username}' AND realm = '#{realm}';"

    when "jabber:iq:last query"
      query = "INSERT INTO logout (`collection-owner`, time) VALUES ( ?, ? );"
      prep = $db.prepare(query)
      prep.bind_params( jid, element.attributes["last"] )
      log_message("Prepared query: #{query}\n\tval: #{jid}\n\t#{element.attributes["last"]}")
      prep.execute
      prep.close

    when "jabber:iq:roster query"
      element.elements.each("item") do |item|
        item_subscription = item.attributes["subscription"]
        if (item_subscription == "to" || item_subscription == "both") then
          item_to = 1
        else
          item_to = 0
        end
        if (item_subscription == "from" || item_subscription == "both") then
          item_from = 1
        else
          item_from = 0
        end
        if (item.attributes["ask"] == "subscribe") then
          item_ask = 1
        else
          # Note: item_ask = 2 isn't possible since jabberd 1.4 doesn't store pending unsubscribe state.
          item_ask = 0
        end

        item_jid = item.attributes["jid"]
        item_name = item.attributes["name"]

        if item_name then
          query = "INSERT INTO `roster-items` (`collection-owner`, jid, name, `to`, `from`, ask) " +
            "VALUES (?, ?, ?, ?, ?, ?);"
          prep = $db.prepare(query)
          prep.bind_params( jid, item_jid, item_name, item_to.to_s, item_from.to_s, item_ask.to_s )
          log_message("Prepared query: #{query}\n\tval: #{jid}\n\t#{item_jid}\n\t#{item_name}\n\t#{item_to.to_s}\n\t#{item_from.to_s}\n\t#{item_ask.to_s}")
        else
          query = "INSERT INTO `roster-items` (`collection-owner`, jid, `to`, `from`, ask) " +
            "VALUES (?, ?, ?, ?, ?);"
          prep = $db.prepare(query)
          prep.bind_params( jid, item_jid, item_to.to_s, item_from.to_s, item_ask.to_s )
          log_message("Prepared query: #{query}\n\tval: #{jid}\n\t#{item_jid}\n\t#{item_to.to_s}\n\t#{item_from.to_s}\n\t#{item_ask.to_s}")
        end
        prep.execute
        prep.close

        item.elements.each("group") do |group|
          if (group.text) then
            query = "INSERT INTO `roster-groups` (`collection-owner`, jid, `group`) " + 
              "VALUES (?, ?, ?);"
            prep = $db.prepare(query)
            prep.bind_params( jid, item_jid, group.text )
            log_message("Prepared query: #{query}\n\tval: #{jid}\n\t#{item_jid}\n\t#{group.text}")
            prep.execute
            prep.close
          end
        end
      end

    when "jabber:x:offline foo"
      element.elements.each("message") do |message|
        query = "INSERT INTO queue (`collection-owner`, `xml`) VALUES (?, ?);"
        prep = $db.prepare(query)
        prep.bind_params( jid, message.to_s )
        log_message("Prepared query: #{query}\n\tval: #{jid}\n\t#{message.to_s}")
        prep.execute
        prep.close
      end

    when "vcard-temp vCard", "vcard-temp vcard" # typo
      query = "INSERT INTO vcard (`collection-owner`) VALUES ( ? );"
      prep = $db.prepare(query)
      prep.bind_params(jid)
      log_message("Prepared query: #{query}\n\tval: #{jid}")
      prep.execute
      prep.close

      def vcard_iter
        yield "fn", "FN"
        yield "nickname", "NICKNAME"
        yield "url", "URL"
        yield "tel", "TEL/NUMBER"
        yield "email", "EMAIL[USERID]/USERID"
        yield "title", "TITLE"
        yield "role", "ROLE"
        yield "bday", "BDAY"
        yield "desc", "DESC"
        yield "n-given", "N/GIVEN"
        yield "n-family", "N/FAMILY"
        yield "adr-street", "ADR/STREET"
        yield "adr-extadd", "ADR/EXTADD"
        yield "adr-locality", "ADR/LOCALITY"
        yield "adr-region", "ADR/REGION"
        yield "adr-pcode", "ADR/PCODE"
        yield "adr-country", "ADR/COUNTRY"
        yield "org-orgname", "ORG/ORGNAME"
        yield "org-orgunit", "ORG/ORGUNIT"
        yield "photo-type", "PHOTO/TYPE"
        yield "photo-binval", "PHOTO/BINVAL"
      end

      vcard_iter { |vcard_table_field, vcard_xpath|
        vcard_field = element.elements[vcard_xpath]
        if (vcard_field and vcard_field.text) then
          query = "UPDATE vcard SET `#{vcard_table_field}` = ? WHERE `collection-owner` = ?;"
          prep = $db.prepare(query)
          prep.bind_params( vcard_field.text, jid )
          log_message("Prepared query: #{query}\n\tval: #{vcard_field.text}\n\t#{jid}")
          prep.execute
          prep.close
        end
      }

    else
      if element.attributes["j_private_flag"] == "1" then
        query = "INSERT INTO private (`collection-owner`, ns, xml) VALUES (?, ?, ?);"
        prep = $db.prepare(query)
        prep.bind_params(jid, element.namespace, element.to_s)
        log_message("Prepared query: #{query}\n\tval: #{jid}\n\t#{element.namespace}\n\t#{element.to_s}")
        prep.execute
        prep.close

      else
        # We ignore these because non-private arbitrary storage is out of the question.
      end # if
    end # case qname
  end # doc.root.elements.each
end # def
  

###### MAIN
# Get options
opts = GetoptLong.new(
  [ "--help", "-h", GetoptLong::NO_ARGUMENT ],
  [ "--debug", "-d", GetoptLong::NO_ARGUMENT ],
  [ "--realm", "-r", GetoptLong::REQUIRED_ARGUMENT ]
)

@options = {}
@options['debug'] = false
@options['realm'] = ""
@options['logpath'] = "/Library/Logs/Migration/jabbermigrator.log"

begin
  opts.each do |opt, arg|
    case opt
      when '--help' || '-h'
        usage()
        exit 0
      when '--debug' || '-d'
        @options['debug'] = true 
      when '--realm' || '-r'
        @options['realm'] = arg
    end # case
  end # opts.each
rescue GetoptLong::InvalidOption => msg
  log_message("ERROR: Invalid Option")
  usage()
  exit 1
end # block

if ARGV[0].nil? || ARGV[1].nil? then
  log_message("ERROR: You must supply both an input directory and output file.")
  usage()
  exit 1
end

@options['indir'] = ARGV[0]
@options['dbfile'] = ARGV[1]

# check to make sure the sqlite destination file is writable
if ! File.writable?(@options['dbfile']) then
  bail("ERROR: cannot write to file #{@options['dbfile']}")
end

# Import the files from specified spool dir
#@infiles = Array.new
indir = Dir.open(@options['indir'])
$db = SQLite3::Database.new( @options['dbfile'] )
$db.busy_timeout(1000)
$db.transaction

indir.each { | infile |
  success = 0
  if infile =~ /^.*\.xml$/
    log_message("Migrating data from file: #{infile}")
    success = migrate_file("#{indir.path}/#{infile}")

    if success == false
      log_message("ERROR: Operation failed, doing db rollback and exiting")
      $db.rollback
      $db.close
      exit
    end
  else
    log_message("NOTICE: line in input file does not have .xml file extension: #{infile.chomp}")
  end
}

$db.commit
$db.close
log_message("Migration completed.")
