module FoundationFunctionsOverrides
  def new_definition
    if @name == 'NSDeallocateObject'
      return 'void NSDeallocateObject(id anObject)'
      
    elsif @name == 'NSCopyObject'
      return 'id NSCopyObject(id anObject, unsigned int extraBytes, NSZone *zone)'
      
    elsif @name == 'NSShouldRetainWithZone'
      return 'BOOL NSShouldRetainWithZone(id anObject, NSZone *requestedZone)'
    end
  end
end