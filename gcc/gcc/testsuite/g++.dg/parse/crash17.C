// PR c++/16716

template <typename> class allocator; 
 
template<typename T> class vector { 
  // With the dg-error on the next line, we are really just trying to
  // check that the message is not an ICE message.
  /* APPLE LOCAL begin mainline 2005-12-27 4431091 */
  typedef typename allocator<T> allocator_type; // { dg-error "expected|invalid" }
  /* APPLE LOCAL end mainline 2005-12-27 4431091 */
}; 
