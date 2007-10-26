module Extras
  # This method checks if a override method (var method) exists.
  # If so it calls it with the optional args array.
  # If it doesn't exist it returns the default alt_result.
  def override_result(alt_result, method, args = [])
    if respond_to? method
      if args.empty?
        override_result = self.send(method)
      else
        override_result = self.send(method, *args)
      end
      return override_result unless override_result.nil?
    end
    return alt_result
  end
end