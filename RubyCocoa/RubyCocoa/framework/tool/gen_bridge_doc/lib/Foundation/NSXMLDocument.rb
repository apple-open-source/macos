module NSXMLDocumentOverrides
  def new_definition
    # All the args are stringed together (so no spaces) in the original ref
    return '- (id)objectByApplyingXSLTString:(NSString *)xslt arguments:(NSDictionary *)arguments error:(NSError **)error' if @name == 'objectByApplyingXSLTString:arguments:error:'
  end
end