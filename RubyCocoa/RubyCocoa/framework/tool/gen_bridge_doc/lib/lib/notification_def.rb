class CocoaRef::NotificationDef < CocoaRef::MethodDef
  def to_rdoc
    str  = "\n"
    unless @description.nil? or @description.empty?
      str += "  # Description:: #{@description.rdocify}\n"
    else
      puts "[WARNING] A `nil` or empty object was encountered as the description for notification #{@name}" if $COCOA_REF_DEBUG
      str += "  # Description:: No description was available/found.\n"
    end
  
    unless @availability.nil? or @availability.empty?
      str += "  # Availability:: #{@availability.rdocify}\n"
    else
      puts "[WARNING] A `nil` or empty object was encountered as the availability for notification #{@name}" if $COCOA_REF_DEBUG
      str += "  # Availability:: No availability was available/found.\n"
    end
  
    str += "  #{@name} = ''\n"
    return str
  end
end
