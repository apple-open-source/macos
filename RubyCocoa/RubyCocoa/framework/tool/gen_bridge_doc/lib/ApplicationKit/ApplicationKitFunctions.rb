module ApplicationKitFunctionsOverrides
  # These are overrides for a extra arg which is '...'
  
  def new_regexp_result_type(res)
    return '' if @name == 'NSBeginAlertSheet' and res.empty?
  end
  
  def new_regexp_result_arg(res)
    return '' if @name == 'NSBeginAlertSheet' and res.empty?
  end
end