#
#  BuildAsset.rb
#  CertificateTool
#
#  Copyright 2012 Apple Inc. All rights reserved.
#

require 'fileutils'

class BuildPKIAsset
    attr_reader :ios_SDK_path
    attr_reader :base_path
    attr_reader :output_directory
    attr_reader :asset_directory
    attr_reader :asset_top_directory
    attr_reader :asset_data_directory
    attr_reader :staging_directory
    attr_reader :info_plist_path
    attr        :verbose
    
    def validate_path(path, isDir = true)
        return false if path.nil? || path.empty?
        return false if !FileTest.exists?(path)
        return false if isDir != FileTest.directory?(path)
        true
    end

    def output_str(str, header = false)
        return if !@verbose
        
        puts "=====================================================" if header
        puts str if !str.nil?
    end   
    
    def ensure_directory(path)
        FileUtils.mkdir_p path if !validate_path(path)
        validate_path(path)
    end
            

    def initialize(input_dir, output_directory, project_path, staging_directory, verbose = false)
        
        @verbose = verbose
        
        output_str(nil, true)
        output_str "In BuildPKIAsset.initialize" 
        output_str "input_dir = #{input_dir}"
        output_str "output_directory = #{output_directory}" 
        output_str "staging_directory = #{staging_directory}"
        output_str(nil, true)
        output_str( " ") 
        
        # Check the input parameter
        if !ensure_directory(input_dir)
            puts "Invalid base directory given: #{input_dir}"
            exit
        end
        
        @base_path =  File.expand_path(input_dir)
        
        @info_plist_path = File.join(File.join(File.expand_path(project_path), "../config"), "Info-Asset.plist")
        
        if !FileTest.exists? @info_plist_path
          puts "Could not find the Info.plist file"
          exit
        end
        
        
        if output_directory.nil? || output_directory.empty?
            puts "No output directory was given"
            exit
        end

        asset_tool_path = `xcodebuild -sdk iphoneos.internal -find assettool`
        if asset_tool_path.nil?
            puts "Unable to find the mobile asset tool in the iPhone SDK"
            exit
        end
        
        
        @output_directory = File.expand_path(output_directory)
        
        @asset_directory = File.join(@output_directory, "Assets")
        ensure_directory(@asset_directory)
        
        @asset_top_directory = File.join(@asset_directory, "SecurityCertificatesAssets")
        ensure_directory(@asset_top_directory)
        
        @asset_data_directory = File.join(@asset_top_directory, "AssetData/PKITrustData")
        ensure_directory(@asset_data_directory)
        
        @staging_directory =   File.expand_path(staging_directory)
        ensure_directory(@staging_directory)
        
        
        output_str(nil, true)
        output_str "@base_path  = #{@base_path }" 
        output_str "@output_directory  = #{@output_directory }" 
        output_str "@asset_directory  = #{@asset_directory }" 
        output_str "@asset_top_directory  = #{@asset_top_directory }" 
        output_str "@asset_data_directory  = #{@asset_data_directory }" 
        output_str "@staging_directory  = #{@staging_directory }" 
        output_str "@info_plist_path = #{@info_plist_path}" 
        output_str "Done with BuildPKIAsset.initialize" 
        output_str(nil, true)
        output_str( " ")  
        
    end
    
    def stage
        
        output_str(nil, true)
        output_str "In BuildPKIAsset.stage" 
        output_str(nil, true)
        output_str( " ")  
        
        #copy over the files into the asset directory
        input_plist_file_path = @info_plist_path
        
        output_str(nil, true)
        output_str "input_plist_file_path = #{input_plist_file_path}"
        
        FileUtils.cp(input_plist_file_path, @asset_top_directory)
        
        output_str "About to copy over the plist files"
        
        # copy all of the necessary files into the asset data directory
        file_list = %w(AppleESCertificates.plist AssetVersion.plist Blocked.plist GrayListedKeys.plist EVRoots.plist certsIndex.data certsTable.data manifest.data)
        file_list.each do |file|
            file_path = File.join(@base_path, file)
            if !FileTest.exists?(file_path)
                output_str(nil, true)
                output_str( " ") 
                puts "#{file_path} is missing in the base directory"
                exit
            end
            FileUtils.cp(file_path, @asset_data_directory)
        end
        
        output_str "Completed copying over the plist files"
        
        output_str "About to call assettool stage" 
        `xcrun -sdk iphoneos.internal assettool stage -p #{@asset_directory} -s #{@staging_directory}`
        output_str "Completed call to assettool stage" 
        output_str(nil, true)
        output_str( " ")
        
        output_str(nil, true)
        output_str "Done with BuildPKIAsset.stage"
        output_str(nil, true)
        output_str( " ")    
    end
    
    def sign
        output_str(nil, true)
        output_str "In BuildPKIAsset.sign" 
        output_str "About to call assettool sign" 
        `xcrun -sdk iphoneos.internal assettool sign -s #{@staging_directory}`
        output_str "Completed call to assettool sign"
        output_str "Done with BuildPKIAsset.sign"
        output_str(nil, true)
        output_str( " ")                       
    end
    
end

@verbose = false

def do_output_str(str, header = false)
    return if !@verbose
    
    puts "=====================================================" if header
    puts str if !str.nil?
end

build_dir = ENV["BUILT_PRODUCTS_DIR"]
project_dir = ENV["PROJECT_DIR"]

do_output_str "Environment variables"
do_output_str " "
do_output_str "build_dir = #{build_dir}"
do_output_str "project_dir = #{project_dir}"
do_output_str(nil, true)
do_output_str(" ")  

input_path = File.join(build_dir, "BuiltAssets")
output_path = File.join(build_dir, "Asset")
staging_path = File.join(build_dir, "staging")
asset_name = "com_apple_MobileAsset_PKITrustServices_PKITrustData"
full_asset_path = File.join(staging_path, asset_name)

do_output_str(nil, true)
do_output_str "Path variables"
do_output_str " "
do_output_str "input_path = #{input_path}"
do_output_str "output_path = #{output_path}"
do_output_str "staging_path = #{staging_path}"
do_output_str "full_asset_path = #{full_asset_path}" 
do_output_str(nil, true)
do_output_str(" ")
   
do_output_str(nil, true)
do_output_str "Creating a BuildPKIAsset object to stage and sign the asset"
b = BuildPKIAsset.new(input_path, output_path, project_dir, staging_path, @verbose)
b.stage
b.sign

do_output_str "Finished with BuildAsset"
do_output_str(nil, true)
do_output_str(" ")

do_output_str(nil, true)
do_output_str "Output Path variables"
do_output_str(" ")
do_output_str "build_dir = #{build_dir}"
do_output_str "full_asset_path = #{full_asset_path}"

#FileUtils.cp_r(full_asset_path, build_dir)

do_output_str "Asset build complete"
do_output_str(nil, true)
do_output_str(" ") 