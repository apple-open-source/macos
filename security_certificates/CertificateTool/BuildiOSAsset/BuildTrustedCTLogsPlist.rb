#
#  BuildTrustCTLogsPlist.rb
#  CertificateTool
#
#  Copyright 2015 Apple Inc. All rights reserved.
#
require 'fileutils'
require 'json'
require 'cfpropertylist'

@verbose = false

def do_output_str(str, header = false)
    return if !@verbose

    puts "=====================================================" if header
    puts str if !str.nil?
end

src_file = ARGV[0]
dst_file = ARGV[1]

do_output_str(nil, true)
do_output_str "src_file = #{src_file}"
do_output_str "dst_file = #{dst_file}"
do_output_str(nil, true)

string = File.read(src_file)

parsed = JSON.parse(string) # returns a hash

do_output_str(parsed)
do_output_str(nil, true)

operators = Hash.new 
parsed["operators"].each do |operator|
  operators[operator["id"]]=operator["name"]
end

A = Array.new

STATE_INCLUDED = 0
STATE_FROZEN = 1
STATE_PENDING = 2
STATE_DISQUALIFIED = 3

parsed["logs"].each do |log|
    if log["state"] != STATE_PENDING then # Skip pending logs
        logEntry = Hash.new;
        logEntry["key"] = CFPropertyList::Blob.new(Base64.decode64(log["key"]))
        logEntry["operator"] = operators[log["operated_by"][0]]
        if log["state"] == STATE_FROZEN then
            logEntry["expiry"] = Time.at(log["final_sth"]["timestamp"]/1000)
        end
        if log["state"] == STATE_DISQUALIFIED then
            logEntry["expiry"] = Time.at(log["disqualified_at"]/1000)
        end
        A.push(logEntry)
    end
end


do_output_str(A)
do_output_str(nil, true)

plist = A.to_plist({:plist_format => CFPropertyList::List::FORMAT_XML, :formatted => true})

do_output_str(plist)
do_output_str(nil, true)


File.write(dst_file, plist)

exit 0
