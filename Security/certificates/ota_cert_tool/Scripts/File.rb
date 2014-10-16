#
#  File.rb
#  ios_ota_cert_tool
#
#  Copyright 2012 Apple Inc. All rights reserved.
#

require 'FileUtils'

class BuildPKIAsset
  attr_reader :ios_SDK_path
  attr_reader :base_path
  attr_reader :output_directory
  attr_reader :asset_directory
  attr_reader :asset_top_directory
  attr_reader :asset_data_directory
  attr_reader :staging_directory
  attr        :verbose
   
  def validate_path(path, isDir = true)
    return false if path.nil? || path.empty?
    return false if !FileTest.exists?(path) 
    return false if isDir != FileTest.directory?(path)
    true
  end
  
  def ensure_directory(path)
    if !FileTest.exists?(path) 
      FileUtils.mkdir_p(path)
    end
  end
  
  def initialize(input_dir, output_directory, staging_directory = "/tmp/staging")
    
    @verbose = true
    
    puts "In BuildPKIAsset.initialize" if @verbose
    puts "input_dir = #{input_dir}" if @verbose
    puts "output_directory = #{output_directory}" if @verbose
    puts "staging_directory = #{staging_directory}" if @verbose
    
    # Check the input parameter
    if !validate_path(input_dir)
        puts "Invalid base directory given: #{input_dir}"
      exit
    end
    
    @base_path =  File.expand_path(input_dir)
    
    if output_directory.nil? || output_directory.empty?
      puts "No output directory was given"
      exit
    end
    
    exit if `xcodebuild -sdk iphoneos.internal -find assettool`.nil?
    
    @output_directory = File.expand_path(output_directory)
    
    @asset_directory = File.join(@output_directory, "Assets")
    ensure_directory(@asset_directory)
    
    @asset_top_directory = File.join(@asset_directory, "SecurityCertificatesAssets")
    ensure_directory(@asset_top_directory)
    
    @asset_data_directory = File.join(@asset_top_directory, "AssetData/PKITrustData")
    ensure_directory(@asset_data_directory) 
    
    @staging_directory =   File.expand_path(staging_directory)
    ensure_directory(@staging_directory) 
    
    
    puts "@base_path  = #{@base_path }" if @verbose
    puts "@output_directory  = #{@output_directory }" if @verbose
    puts "@asset_directory  = #{@asset_directory }" if @verbose
    puts "@asset_top_directory  = #{@asset_top_directory }" if @verbose
    puts "@asset_data_directory  = #{@asset_data_directory }" if @verbose
    puts "@staging_directory  = #{@staging_directory }" if @verbose
    
    puts "Done with BuildPKIAsset.initialize" if @verbose
        
  end
  
  def stage
    
    puts "In BuildPKIAsset.stage" if @verbose
    
    #copy over the files into the asset directory
    input_plist_file_path = File.join(@base_path, "Info.plist")
    if !FileTest.exists?(input_plist_file_path)
      puts "The asset data Info.plist file is missing #{input_plist_file_path}"
      exit
    end
    
    puts "input_plist_file_path = #{input_plist_file_path}" if @verbose
    
    FileUtils.cp(input_plist_file_path, @asset_top_directory)
    
    puts "About to copy over the plist files" if @verbose
    
    # copy all of the necessary files into the asset data directory
    file_list = %w(EVRoots.plist certs.plist revoked.plist Manifest.plist distrusted.plist roots.plist)
    file_list.each do |file|
      file_path = File.join(@base_path, file)
      if !FileTest.exists?(file_path)
        puts "#{file_path} is missing in the base directory"
        exit
      end
      FileUtils.cp(file_path, @asset_data_directory)
    end
    
    puts "Completed copying over the plist files" if @verbose
    
    puts "About to call assettool stage" if @verbose
    `xcrun -sdk iphoneos.internal assettool stage -p #{@asset_directory} -s #{@staging_directory}`
    puts "Completed call to assettool stage" if @verbose
    
    puts "Done with BuildPKIAsset.stage" if @verbose
  end
  
  def sign
    puts "In BuildPKIAsset.sign" if @verbose
    puts "About to call assettool sign" if @verbose
    `xcrun -sdk iphoneos.internal assettool sign -s #{@staging_directory}`
    puts "Completed call to assettool sign" if @verbose
    puts "Done with BuildPKIAsset.sign" if @verbose
  end
    
end

=begin
  The following code is here ONLY for testing
=end

#b = BuildPKIAsset.new(File.expand_path("~/cert_out"), File.expand_path("~/bobby_cert"))
#b.stage
#b.sign
#puts "That's all folks@"




