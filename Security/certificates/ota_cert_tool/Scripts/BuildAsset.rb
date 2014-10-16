#
#  BuildAsset.rb
#  ios_ota_cert_tool
#
#  Copyright 2012 Apple Inc. All rights reserved.
#

require "FileUtils"

build_dir = ENV["BUILT_PRODUCTS_DIR"]
project_dir = ENV["PROJECT_DIR"]
input_path = File.expand_path("~/tmp/asset_out")
output_path = File.join(build_dir, "Asset")
script_path = File.join(project_dir, "Scripts/File.rb")
staging_path = File.expand_path("~/tmp/staging")
asset_name = "com_apple_MobileAsset_PKITrustServices_PKITrustData"
full_asset_path = File.join(staging_path, asset_name)


require script_path


puts "Creating the BuildPKIAsset instance"
b = BuildPKIAsset.new(input_path, output_path, staging_path)
puts "Calling stage on the BuildPKIAsset instance"
b.stage
puts "Calling stage on the BuildPKIAsset sign"
b.sign

puts "Finished with BuildPKIAsset"

puts "build_dir = #{build_dir}"
puts "full_asset_path = #{full_asset_path}"

FileUtils.cp_r(full_asset_path, build_dir)

puts "That's all folks!"


