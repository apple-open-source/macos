#
#  MyDocument.rb
#  QTKitPlayer
#
#  Created by Laurent Sansonetti on 12/15/06.
#  Copyright (c) 2006 Apple Computer. All rights reserved.
#

class MyDocument < NSDocument
  ib_outlet :mMovieView

  def windowNibName
    return 'MyDocument'
  end

  def windowControllerDidLoadNib(aController)
    super_windowControllerDidLoadNib(aController)
    
    if fileName
      movie = QTMovie.alloc.initWithFile_error(fileName, nil)
      movie.setAttribute_forKey(NSNumber.numberWithBool(true), QTMovieEditableAttribute)
      @mMovieView.setMovie(movie)
    end
  end

  def dataRepresentationOfType(type)
    return nil
  end
    
  def loadDataRepresentation_ofType(data, type)
    return true
  end

  def saveDocument(sender)
    @mMovieView.movie.updateMovieFile
    updateChangeCount(NSChangeCleared)
  end
  ib_action :saveDocument
end
