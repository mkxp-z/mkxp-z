#include "mkxp_z.h"
#include "binding-mri.h"

extern "C" {
RUBY_FUNC_EXPORTED void
Init_mkxp_z(void)
{
    initBindings();
}
}