module CIFilterShapeOverrides
  def new_definition
    case @name
    when 'shapeWithRect:'
      return '+ (CIFilterShape *)shapeWithRect:(CGRect)r'
    when 'insetByX:Y:'
      return '- (void)insetByX:(int)dx Y:(int)dy'
    end
  end
end