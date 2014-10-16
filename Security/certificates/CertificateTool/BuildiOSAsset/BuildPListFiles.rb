#
#  BuildPListFiles.rb
#  CertificateTool
#
#  Copyright 2012 Apple Inc. All rights reserved.
#

@verbose = false

def do_output_str(str, header = false)
    return if !@verbose
    
    puts "=====================================================" if header
    puts str if !str.nil?
end

do_output_str(nil, true)
do_output_str(" ")
do_output_str "Entering BuildPlistFiles.rb"
do_output_str(nil, true)
do_output_str(" ")

build_dir = ENV["BUILT_PRODUCTS_DIR"]
sdk_name = ENV["SDK_NAME"]
top_level_directory = ENV["PROJECT_DIR"]

do_output_str(nil, true)
do_output_str("Environment variables")
do_output_str(" ")

do_output_str "build_dir = #{build_dir}"
do_output_str "sdk_name = #{sdk_name}"
do_output_str "top_level_directory = #{top_level_directory}"
do_output_str(nil, true)
do_output_str(" ")

top_level_directory = File.join(top_level_directory, "..")
output_directory = File.join(build_dir, "asset_out")
tool_path = File.join(build_dir, "CertificateTool")

do_output_str(nil, true)
do_output_str("Path variables")
do_output_str "top_level_directory = #{top_level_directory}"
do_output_str "output_directory = #{output_directory}"
do_output_str "tool_path = #{tool_path}"
do_output_str(nil, true)
do_output_str(" ")

cmd_str = tool_path + " --top_level_directory " + "'" + top_level_directory + "' " + " --output_directory " + "'" + output_directory + "'"
do_output_str(nil, true)
do_output_str "Executing command: #{cmd_str}"
do_output_str(nil, true)
do_output_str(" ")

`#{cmd_str}`

do_output_str(nil, true)
do_output_str "Completed BuildPlistFiles.rb"
do_output_str(nil, true)
do_output_str(" ")


    