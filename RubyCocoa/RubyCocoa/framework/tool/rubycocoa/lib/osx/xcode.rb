# vim:ts=2:sw=2:expandtab:

require "digest/sha1"
require "osx/cocoa"
include OSX

class XcodeProject
  class XcodeProjectError < StandardError; end

  class Group
    def initialize(proj, id)
      @proj, @dic = proj, proj[id]
    end

    def [](key)
      @dic[key]
    end

    def inspect
      "#<Group #{self["name"]}>"
    end

    def add_file(type, path, tree)
      id = @proj.add_object({
        "isa"               => "PBXFileReference",
        "lastKnownFileType" => type.to_s,
        "path"              => path.to_s,
        "sourceTree"        => tree.to_s,
      })
      @dic["children"] = NSArray.alloc.initWithArray(@dic["children"].to_a << id)
      id
    end

    def files
      @dic["children"].map {|i| @proj[i] }
    end
  end

  attr_accessor :objects
  attr_reader :rootObject, :plist

  def initialize(bundle_path)
    @plist_path = "#{bundle_path}/project.pbxproj"
    @plist = NSPropertyListSerialization.objc_send(
      :propertyListFromData, NSData.alloc.initWithContentsOfFile(@plist_path),
      :mutabilityOption, NSPropertyListMutableContainersAndLeaves,
      :format, nil,
      :errorDescription, nil
    )

    raise XcodeProjectError unless @plist

    @objects = @plist["objects"]
    @rootObject = self[@plist["rootObject"]]
  end

  def [](obj_id)
    @objects[obj_id.to_s]
  end

  def add_object(dic)
    id = Digest::SHA1.hexdigest(dic.inspect)[0, 24].upcase
    @plist["objects"][id] = NSDictionary.alloc.initWithDictionary(dic)
    id
  end

  def add_file_to_resouce_phase(id)
    resouce_phase = @objects.find {|k,v| v["isa"] == "PBXResourcesBuildPhase" }[1]
    resouce_phase["files"] = NSArray.alloc.initWithArray(resouce_phase["files"].to_a << add_object({
      "isa" => "PBXBuildFile",
      "fileRef" => id
    }))
  end

  def groups
    mainGroup = self[@rootObject["mainGroup"]]
    Hash[*mainGroup["children"].map {|i|
      g = Group.new(self, i)
      [g["name"].to_s, g]
    }.select {|i| i[1]["isa"].to_s == "PBXGroup"}.flatten]
  end

  def save
    File.open(@plist_path, "w") do |f|
      f.puts @plist
    end
  end
end

#plist.each do |k,v|
# p k.to_s
# case v
# when NSCFString
#   p v.to_s
#
# when NSCFDictionary
#   v.each do |k,v|
#     p k.to_s
#   end
# end
# puts
#end


if __FILE__ == $0
  proj = XcodeProject.new("#{ENV["HOME"]}/tmp/testcocoa/testcocoa.xcodeproj")
  p proj.groups
  p proj.objects.find { |k, v|
    v["isa"] == "PBXFileReference" and v["path"] == "path.rb"
  }
  #p proj.groups["Classes"].files.find {|i| i["path"].to_s }
#  id = proj.groups["Classes"].add_file("text.script.ruby", "path.rb", "<group>")
#  proj.add_file_to_resouce_phase(id)
#  proj.save
end
#mainGroup = proj[proj.rootObject["mainGroup"]]
#mainGroup["children"].each do |k|
# obj = proj[k]
# puts obj["isa"].to_s
# case obj["isa"].to_s
# when "PBXFileReference"
#   p obj["path"].to_s
# when "PBXGroup"
#   p obj["name"].to_s
#   p obj
#   if obj["name"].to_s == "Other Sources"
#     obj["children"].each do |c|
#       p proj[c]
#     end
#   end
# end
# puts
#end
#
#require "digest/sha1"
#
#p Digest::SHA1.hexdigest('')[0, 24].upcase

