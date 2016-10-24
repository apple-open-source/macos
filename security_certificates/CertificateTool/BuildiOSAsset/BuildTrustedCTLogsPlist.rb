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


build_dir = ENV["BUILT_PRODUCTS_DIR"]
top_level_directory = ENV["PROJECT_DIR"]

src_file = File.join(top_level_directory, "../certificate_transparency/log_list.json")
dst_file = File.join(build_dir, "BuiltAssets/TrustedCTLogs.plist")

do_output_str(nil, true)
do_output_str "build_dir = #{build_dir}"
do_output_str "top_level_directory = #{top_level_directory}"
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

parsed["logs"].each do |log|
    logEntry = Hash.new;
    logEntry["key"] = CFPropertyList::Blob.new(Base64.decode64(log["key"]))
    logEntry["operator"] = operators[log["operated_by"][0]]
    if log["expiry"] then
        logEntry["expiry"] = DateTime.parse(log["expiry"])
    end
    A.push(logEntry)
end


do_output_str(A)
do_output_str(nil, true)

plist = A.to_plist({:plist_format => CFPropertyList::List::FORMAT_XML, :formatted => true})

do_output_str(plist)
do_output_str(nil, true)


File.write(dst_file, plist)

exit 0
