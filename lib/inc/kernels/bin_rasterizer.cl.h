#pragma once

#include "../general/predefs.h"

namespace nr
{

namespace __internal
{

namespace clcode
{

const string bin_rasterizer = R"__CODE__(

// -------------------------------------- Types and defines -------------------------------------- 

// DON'T CHANGE THIS 
#define BATCH_COUNT (256)

// ----------------------------------------------------------------------------

// -------------------------------------- Utilities --------------------------------------

bool is_point_in_bin(const uint x, const uint y, const Bin bin)
{
    return bin.x <= x && x < bin.x + bin.width && bin.y <= y && y < bin.y + bin.height; 
}

float min3(const float a, const float b, const float c)
{
    return min(a, min(b, c));
}

float max3(const float a, const float b, const float c)
{
    return max(a, max(b, c));
}

float4 mk_triangle_bounding_rect(const generic float x[3], const generic float y[3])
{
    return (float4)(min3(x[0], x[1], x[2]), min3(y[0], y[1], y[2]), max3(x[0], x[1], x[2]), max3(y[0], y[1], y[2]));
}

bool is_point_in_bounds(const float2 p, const float4 bounds)
{
    const float2 b_min = bounds.xy;
    const float2 b_max = bounds.zw;
    return b_min.x <= p.x && p.x <= b_max.x && b_min.y <= p.y && p.y <= b_max.y;
}

bool is_rect_intersects_bounds(const float4 rect, const float4 bounds)
{
    return is_point_in_bounds(rect.xy, bounds) ||
           is_point_in_bounds(rect.xw, bounds) ||
           is_point_in_bounds(rect.zy, bounds) ||
           is_point_in_bounds(rect.zw, bounds);
}

bool is_triangle_in_bin(const generic float x[3], const generic float y[3], const Bin bin, const ScreenDimension dim)
{
    float4 triangle_bounds = mk_triangle_bounding_rect(x, y);
    float4 bin_bounds = (float4)(
        axis_ndc_from_screen(bin.x, dim.width), axis_ndc_from_screen(bin.y, dim.height),
        axis_ndc_from_screen(bin.x + bin.width, dim.width), axis_ndc_from_screen(bin.y + bin.height, dim.height));   
    
    // DEBUG_ONCE4("Triangle bounds: [(%f, %f), (%f, %f)]\n", triangle_bounds.x, triangle_bounds.y, triangle_bounds.z, triangle_bounds.w);
    // DEBUG_ONCE4("Bin bounds: [(%f, %f), (%f, %f)]\n", bin_bounds.x, bin_bounds.y, bin_bounds.z, bin_bounds.w);

    return is_rect_intersects_bounds(triangle_bounds, bin_bounds) || is_rect_intersects_bounds(bin_bounds, triangle_bounds);
}

Bin make_bin(const ScreenDimension dim, const uint index_x, const uint index_y, const uint bin_width, const uint bin_height)
{
    Bin ret;
    ret.x = index_x * bin_width;
    ret.y = index_y * bin_height;
    ret.width = min(dim.width - ret.x, bin_width);
    ret.height = min(dim.height - ret.y, bin_height); 
    return ret;
}

event_t reduce_triangle_buffer(
    const global Triangle* triangles, 
    const uint triangle_count, 
    const uint offset, 
    event_t event, 
    local float* result_x, 
    local float* result_y)
{
    local float* dest_x = (local float*) result_x;
    local float* dest_y = (local float*) result_y;
    const global float* src_base = ((const global float*) triangles) + offset; 

    const uint raw_copy_count = triangle_count * 3; // one for each point in the triangle buffer

    event_t ret = async_work_group_strided_copy(dest_x, src_base, raw_copy_count, RENDER_DIMENSION, 0);  // Copying x values
    return async_work_group_strided_copy(dest_y, src_base + 1, raw_copy_count, RENDER_DIMENSION, ret);   // Copying y values
}

// ----------------------------------------------------------------------------

global atomic_uint g_batch_index;

kernel void bin_rasterize(
    const global Triangle* triangle_data,
    const uint triangle_count,
    const ScreenDimension dim,
    const BinQueueConfig config,
    global bool* has_overflow,
    global Index* bin_queues)
{
    DEBUG(if (get_num_groups(0) * get_num_groups(1) > MAX_WORK_GROUP_COUNT) return);

    // DEBUG_ONCE("Starting bin rasterizer\n");

    local float reduced_triangles_x[BATCH_COUNT * RENDER_DIMENSION];
    local float reduced_triangles_y[BATCH_COUNT * RENDER_DIMENSION];
    local uint current_batch_index;

    // Workaround for that wierd compiler bug
    private const ScreenDimension screen_dim = dim;
    
    private uint index_x = get_local_id(0);
    private uint index_y = get_local_id(1);

    private bool is_init_manager = !index_x && !index_y;

    private event_t batch_acquisition = 0;
    
    private uint bin_queue_index = 0;
    private const uint bins_count_x = ceil(((float) screen_dim.width) / config.bin_width);
    private const uint bins_count_y = ceil(((float) screen_dim.height) / config.bin_height);
    private uint bin_queue_base = (config.queue_size + 1) * (bins_count_x * bins_count_y * (get_group_id(1) * get_num_groups(0) + get_group_id(0)) + bins_count_x * index_y + index_x); 
    private uint current_queue_index = bin_queue_base + 1;

    private uint batch_actual_size;

    private const Bin current_bin = make_bin(screen_dim, index_x, index_y, config.bin_width, config.bin_height);

    if (!get_global_id(0) && !get_global_id(1))
    {
        atomic_init(&g_batch_index, 0);
        *has_overflow = false;
    }

    if (is_init_manager)
    {
        current_batch_index = 0;
    }

    // wait for LOCAL batch index initialization
    work_group_barrier(CLK_LOCAL_MEM_FENCE);

    // wait for GLOBAL batch index initialization
    work_group_barrier(CLK_GLOBAL_MEM_FENCE);

    bin_queues[bin_queue_base] = 1;

    for(;;)
    {
        work_group_barrier(CLK_LOCAL_MEM_FENCE);

        // Aquire a batch (update the local and global batch index)
        if (is_init_manager)
        {
            current_batch_index = atomic_fetch_add(&g_batch_index, BATCH_COUNT);
        }

        work_group_barrier(CLK_LOCAL_MEM_FENCE);

        if (*has_overflow)
        {
            return;
        }
        
        // no more batches to process, this work group has no more work to do
        if (current_batch_index >= triangle_count)
        {
            return;
        }
        
        batch_actual_size = min((uint) BATCH_COUNT, triangle_count - current_batch_index);

        batch_acquisition = reduce_triangle_buffer(triangle_data, batch_actual_size, current_batch_index, 0, reduced_triangles_x, reduced_triangles_y);
        wait_group_events(1, &batch_acquisition);
                
        for (private uint i = 0; i < batch_actual_size; ++i)
        {
            if (
                is_triangle_in_bin(
                    reduced_triangles_x + i * 3, 
                    reduced_triangles_y + i * 3, 
                    current_bin, 
                    screen_dim))
            {   
                bin_queues[current_queue_index++] = current_batch_index + i;
                bin_queues[bin_queue_base] = 0;
                // DEBUG_MESSAGE3("Triangle found at (%d, %d) - idx %d\n", current_bin.x, current_bin.y, current_batch_index + i);
            }

            // An overflowing queue was detected
            if (current_queue_index >= config.queue_size + bin_queue_base + 1)
            {
                *has_overflow = true;
                break;
            }
        }

        if (current_queue_index != bin_queue_base + 1 + config.queue_size)
        {
            bin_queues[current_queue_index] = 0;
        }
    }     
}

// -------------------------------------- Tests --------------------------------------

#ifdef _TEST_BINNING

#ifndef TRIANGLE_TEST_COUNT
    #error "TRIANGLE_TEST_COUNT has to be defined in binning test configuration"
#endif 

kernel void is_triangle_in_bin_test(
    const global float triangle_x[3],
    const global float triangle_y[3], 
    const Bin bin,
    const ScreenDimension dim,
    global bool* result)
{
    *result = is_triangle_in_bin(triangle_x, triangle_y, bin, dim);
}

kernel void reduce_triangle_buffer_test(
    const global Triangle triangle_data[TRIANGLE_TEST_COUNT],
    const uint offset,
    global NDCPosition result[TRIANGLE_TEST_COUNT * 3])
{
    local float res_x[TRIANGLE_TEST_COUNT * 3];
    local float res_y[TRIANGLE_TEST_COUNT * 3];
    event_t wait = reduce_triangle_buffer(triangle_data, TRIANGLE_TEST_COUNT, offset, 0, res_x, res_y);
    wait_group_events(1, &wait);

    if (get_global_id(0) == 0)
    {
        __attribute__((opencl_unroll_hint))
        for (uint i = 0; i < TRIANGLE_TEST_COUNT * 3; ++i)
        {
            result[i].x = res_x[i];
            result[i].y = res_y[i];
        }
    }
}

#endif // _TEST_BINNING

)__CODE__";

}

}

}