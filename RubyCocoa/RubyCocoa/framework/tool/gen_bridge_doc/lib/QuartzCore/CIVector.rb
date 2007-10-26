module CIVectorOverrides
  def new_definition
    case @name
    when 'initWithValues:count:'
      return '- (id)initWithValues:(const float *)values count:(size_t)count'
    when 'initWithX:'
      return '- (id)initWithX:(float)x'
    when 'initWithX:Y:'
      return '- (id)initWithX:(float)x Y:(float)y'
    when 'initWithX:Y:Z:'      
      return '- (id)initWithX:(float)x Y:(float)y Z:(float)z'
    when 'initWithX:Y:Z:W:'      
      return '- (id)initWithX:(float)x Y:(float)y Z:(float)z W:(float)w'
    end
  end
end