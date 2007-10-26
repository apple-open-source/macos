// { dg-do compile }

namespace Out {
  namespace In {
  }
}

// APPLE LOCAL mainline 2006-10-13 3904173
class Klasse : public Out::In {  // { dg-error ".*" "" }
};
