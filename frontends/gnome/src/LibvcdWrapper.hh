#ifndef __LIBVCDWRAPPER_HH__
#define __LIBVCDWRAPPER_HH__

namespace libvcd {
#include "vcd.h"
}

class VcdObj {
  libvcd::VcdObj *m_obj;

 public:
  typedef libvcd::vcd_type_t type_t;
  
  VcdObj (type_t type)
    : m_obj (vcd_obj_new (type))
    {
      assert (m_obj != 0);
    }

  ~VcdObj ()
    {
      vcd_obj_destroy (m_obj);
    }
};

#endif // __LIBVCDWRAPPER_HH__
