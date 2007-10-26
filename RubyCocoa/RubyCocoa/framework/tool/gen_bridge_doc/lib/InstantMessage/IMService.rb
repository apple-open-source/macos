module IMServiceOverrides
  def new_definition
    return '- (NSDictionary *)infoForScreenName:(NSString *)screenName' if @name == 'infoForScreenName:'
  end
end