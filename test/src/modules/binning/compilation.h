#pragma once

#include <base/Module.h>

#include "bin_utils.h"

using namespace nr;
using namespace testing;

TEST(Binning, Compilation)
{
    testCompilation(Module::Options{Module::CL_VERSION_12 }, "production 3d", {clcode::base, clcode::bin_rasterizer});    
    testCompilation(Module::Options{Module::CL_VERSION_12, Module::DEBUG, TEST_BINNING, mkTriangleTestCountMacro(3)}, "debug/testing 3d", {clcode::base, clcode::bin_rasterizer});
}

