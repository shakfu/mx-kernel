// Out-of-line special members for t_kernel_impl.
//
// types.h forward-declares xeus::xkernel and xeus::xcontext so that Max-facing
// code does not have to pull in the xeus headers. unique_ptr still needs the
// complete type to destroy them, so the destructor is defined here, where the
// full definitions are visible.

#include "types.h"

#include "interpreter.h"
#include "xeus/xeus_context.hpp"
#include "xeus/xkernel.hpp"

namespace mx {

t_kernel_impl::t_kernel_impl() = default;
t_kernel_impl::~t_kernel_impl() = default;

} // namespace mx
