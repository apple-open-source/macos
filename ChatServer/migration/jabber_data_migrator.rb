#!/usr/bin/ruby -rubygems
# This script reads in a line-delimited list of jabberd 1.x XML spool files (1 filename per line) and migrates
# the data in each spool file to a SQLite database file for use with Jabberd 2.x
# The SQLite database file must already exist with the standard jaberd 2.x schema (as created by db-setup.sqlite)
#
# Copyright 2006 Apple Computer Inc. All rights reserved.
#

require "rexml/document"
require "getoptlong"
require "sqlite3"

def usage()
	puts "\nusage: #{File.basename($0)} [opts] <input file> <output file>\n"
	puts "Arguments:"
	puts "-h, --help\tDisplay usage info"
	puts "-d, --debug\tEnable debug mode"
	puts "-r, --realm\tSpecify a realm to use for JIDs.  Otherwise it will be taken from the input spool file path."
	puts ""
	puts "<input file>: Path of file containing a list of jabberd 1.x XML spool files for migration."
	puts "\tThe file should contain filenames delimited by line endings.  Ex:"
	puts "\t\t user1.xml"
	puts "\t\t user2.xml"
	puts "\t\t user3.xml"
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

def migrate_file(file)
	db = SQLite3::Database.new( @options['dbfile'] )
	db.busy_timeout(1000)
	if @options['realm'] == "" then
		if file =~ /^.*\/(.+)\/(.+).xml$/ then
			realm = $1
			username = $2
		else
			log_message("ERROR: Cannot get realm and/or username from paths provided, for line:\n\t#{file}")
			db.close
			return
		end
	else
		if file =~ /^.*\/(.+).xml$/ then
			realm = @options['realm']
			username = $1
		else
			log_message("ERROR: Cannot get username from paths provided, for line: #{file}")
            db.close
            return
		end
	end	
	jid = username +  "@" + realm
	
	log_message("Processing file for user: #{jid}...")

	if ! File.exist?(file) then
		log_message("ERROR: File does not exist: #{file}")
		db.close
		return
	end
	spoolfile = File.new(file, 'r')
	doc = REXML::Document.new(spoolfile)

	query = "INSERT INTO active (`collection-owner`) VALUES ('#{quote(jid)}');"
	log_message("Preparing query: #{query}")
	prep = db.prepare(query)
	prep.execute
	prep.close

	#puts db.prepare("INSERT INTO authreg (username, realm) VALUES ('#{username}', '#{realm}');")
	doc.root.elements.each do |element|
		qname = element.name
		if element.namespace != nil then
			qname = element.namespace + " " + qname
		end

		case qname
		when "jabber:iq:auth:0k zerok"
			#puts "UPDATE authreg SET hash = '#{element.elements["hash"].text}', token = '#{element.elements["token"].text}', " +
				"sequence = #{element.elements["sequence"].text} WHERE username = '#{username}' AND realm = '#{realm}';"

		when "jabber:iq:auth password"
			#puts "UPDATE authreg SET `password` = '#{element.text.nil? ? '' : element.text}' " +
				"WHERE username = '#{username}' AND realm = '#{realm}';"

		when "jabber:iq:last query"
			query = "INSERT INTO logout (`collection-owner`, time) VALUES ('#{quote(jid)}', #{quote(element.attributes["last"])});"
			log_message("Preparing query: #{query}")
    		prep = db.prepare(query)
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
						"VALUES ('#{quote(jid)}', '#{quote(item_jid)}', '#{quote(item_name)}', " +
						"#{quote(item_to.to_s)}, #{quote(item_from.to_s)}, #{quote(item_ask.to_s)});"
				else
					query = "INSERT INTO `roster-items` (`collection-owner`, jid, `to`, `from`, ask) " +
						"VALUES ('#{quote(jid)}', '#{quote(item_jid)}', " +
						"#{quote(item_to.to_s)}, #{quote(item_from.to_s)}, #{quote(item_ask.to_s)});"
				end
				log_message("Preparing query: #{query}")
				prep = db.prepare(query)
				prep.execute
				prep.close

				item.elements.each("group") do |group|
					if (group.text) then
						query = "INSERT INTO `roster-groups` (`collection-owner`, jid, `group`) " + 
							"VALUES ('#{quote(jid)}', '#{quote(item_jid)}', '#{quote(group.text)}');"
						log_message("Preparing query: #{query}")
						prep = db.prepare(query)
						prep.execute
						prep.close

					end
				end
			end

		when "jabber:x:offline foo"
			element.elements.each("message") do |message|
				query = "INSERT INTO queue (`collection-owner`, `xml`) VALUES ('#{quote(jid)}', '#{quote(message.to_s)}');"
				log_message("Preparing query: #{query}")
				prep = db.prepare(query)
				prep.execute
				prep.close

			end

		when "vcard-temp vCard", "vcard-temp vcard" # typo
			query = "INSERT INTO vcard (`collection-owner`) VALUES ('#{quote(jid)}');"
			log_message("Preparing query: #{query}")
			prep = db.prepare(query)
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
			end

			vcard_iter { |vcard_table_field, vcard_xpath|
				vcard_field = element.elements[vcard_xpath]
				if (vcard_field and vcard_field.text) then
					query = "UPDATE vcard SET `#{vcard_table_field}` = '#{quote(vcard_field.text)}' " +
						"WHERE `collection-owner` = '#{quote(jid)}';"
					log_message("Preparing query: #{query}")
					prep = db.prepare(query)
					prep.execute
					prep.close
				end
			}

		else
			if element.attributes["j_private_flag"] == "1" then
				query = "INSERT INTO private (`collection-owner`, ns, xml) VALUES ('#{quote(jid)}', '#{quote(element.namespace)}', '#{quote(element.to_s)}');"
				log_message("Preparing query: #{query}")
				prep = db.prepare(query)
				prep.execute
				prep.close

			else
				# We ignore these because non-private arbitrary storage is out of the question.
			end # if
		end # case qname
	end # doc.root.elements.each
	db.close
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
	log_message("ERROR: You must supply both an input and output file.")
	usage()
	exit 1
end

@options['infile'] = ARGV[0]
@options['dbfile'] = ARGV[1]

# Read in the list of input files
@infiles = Array.new
infile = File.open(@options['infile'], "r")
while line = infile.gets
	if line =~ /^.*\.xml$/
		@infiles.push(line.chomp)
	else
		log_message("ERROR: line in input file does not have .xml file extension: #{line.chomp}")
	end
end

log_message("Read filenames from input file:")
@infiles.each {|spoolfile| log_message(spoolfile)}

# check to make sure the sqlite destination file is writable
if ! File.writable?(@options['dbfile']) then
    bail("ERROR: cannot write to file #{@options['dbfile']}")
end

@infiles.each { |spoolfile|
	migrate_file(spoolfile)
}

log_message("Migration completed.")
