#
#  BuildPlistFiles.rb
#  ios_ota_cert_tool
#
#  Copyright 2012 Apple Inc. All rights reserved.
#

puts "Entering BuildPlistFiles.rb"

build_dir = ENV["BUILT_PRODUCTS_DIR"]
executable_path = ENV["EXECUTABLE_PATH"]
sdk_name = ENV["SDK_NAME"]
top_level_directory = ENV["PROJECT_DIR"]


top_level_directory = File.join(top_level_directory, "..")

output_directory = File.expand_path("~/tmp/asset_out")

tool_path = File.join(build_dir, "ios_ota_cert_tool")

#sdk_name = sdk_name.nil? ? " " : "-sdk #{sdk_name} "


#cmd_str = "xcrun " + sdk_name + "-run ios_ota_cert_tool --top_level_directory " + "'" + top_level_directory + "' " + " --output_directory " + "'" + output_directory + "'"

cmd_str = tool_path + " --top_level_directory " + "'" + top_level_directory + "' " + " --output_directory " + "'" + output_directory + "'"

puts "Executing command: #{cmd_str}"
exec cmd_str





