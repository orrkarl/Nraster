#pragma once

#include "../predefs.h"

#include <functional>
#include <vector>

#include "Device.h"
#include "DeviceType.h"
#include "Exceptions.h"
#include "Platform.h"
#include "Wrapper.h"

namespace nr::base {

CL_TYPE_CREATE_EXCEPTION(Context, CLApiException);

/**
 * @brief bare wrapper for cl_context, for a more comfortable interface.
 *
 * The OpenCL context, behind the stages, is a container for many of the basic OpenCL components, such as
 * buffers or programs.
 */
class Context {
public:
    /**
     * @brief Construct a context object, bound to some devices with certain properties
     *
     * Refer to the OpenCL clCreateContext documentation for detailed explanation of the context properties
     *
     * @note wraps clCreateContext
     * @note if devices has more than MAX_UINT values, this function will throw with status CL_INVALID_VALUE
     *
     * @param       platform    platform targeted by this context
     * @param       devices     devices bound to the context
     */
    Context(Platform& platform, std::vector<DeviceView>& devices);

    /**
     * @brief an 'easy to use' constructor for a Context. Using the given properties and collects all of the
     * Devices of a certain type
     *
     * Refer to the OpenCL clCreateContext documentation for detailed explanation of the context properties
     * @note wraps clCreateContextFromType
     * @param       platform    platform targeted by this context
     * @param       deviceType  type of collected devices. Will default to GPU.
     */
    explicit Context(Platform& platform, DeviceTypeBitField deviceType = DeviceTypeFlag::GPU);

    /**
     * @brief provides access to the underlying OpenCL context
     *
     * @return cl_context underlying context
     */
    cl_context rawHandle();

private:
    static cl_context createFromDeviceList(Platform& platform, std::vector<DeviceView>& devices);

    struct ContextTraits {
        using Type = cl_context;
        static constexpr auto release = clReleaseContext;
    };

    UniqueWrapper<ContextTraits> m_context;
};

}