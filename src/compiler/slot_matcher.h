// slot_matcher.h
#ifndef SLOT_MATCHER_H
#define SLOT_MATCHER_H
#include "utilities/ykobject.h"
namespace yaksha {
  /**
   * Match a given object argument with a data type
   */
  struct slot_matcher {
    virtual ~slot_matcher() = default;
    /**
     * Can we pass this argument to a slot of given data type?
     * @param arg
     * @param datatype
     * @return
     */
    virtual bool slot_match(const ykobject &arg, ykdatatype *datatype) = 0;
    /**
     * Get a datatype object for a function
     * @param arg argument
     * @return nullptr if failed, else a valid datatype
     */
    virtual ykdatatype *function_to_datatype(const ykobject &arg) = 0;
  };
}// namespace yaksha
#endif