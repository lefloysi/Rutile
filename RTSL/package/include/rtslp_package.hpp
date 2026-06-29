#ifndef RUTILE_BACKEND_TOOLS_RTSLP_PACKAGE_HPP
#define RUTILE_BACKEND_TOOLS_RTSLP_PACKAGE_HPP

#include "rutile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rtsl {

enum class ir_op : u16 {
    nop = 0,
    type_void,
    type_bool,
    type_int,
    type_uint,
    type_float,
    type_vector,
    type_matrix,
    type_struct,
    type_pointer,
    type_array,
    type_function,
    type_image,
    type_sampler,
    type_sampled_image,
    constant_bool,
    constant_int,
    constant_uint,
    constant_float,
    constant_composite,
    variable,
    load,
    store,
    access_chain,
    composite_construct,
    composite_extract,
    composite_insert,
    vector_shuffle,
    f_add,
    f_sub,
    f_mul,
    f_div,
    f_mod,
    f_negate,
    i_add,
    i_sub,
    i_mul,
    s_div,
    u_div,
    s_mod,
    u_mod,
    vector_times_scalar,
    matrix_times_scalar,
    matrix_times_vector,
    matrix_times_matrix,
    dot,
    cross,
    f_ord_equal,
    f_ord_not_equal,
    f_ord_less,
    f_ord_less_equal,
    f_ord_greater,
    f_ord_greater_equal,
    i_equal,
    i_not_equal,
    s_less,
    s_less_equal,
    s_greater,
    s_greater_equal,
    u_less,
    u_less_equal,
    u_greater,
    u_greater_equal,
    logical_and,
    logical_or,
    logical_not,
    convert_f_to_u,
    convert_f_to_s,
    convert_s_to_f,
    convert_u_to_f,
    bitcast,
    label,
    branch,
    branch_conditional,
    selection_merge,
    loop_merge,
    return_op,
    return_value,
    function_parameter,
    function_call,
    sampled_image,
    image_sample_implicit_lod,
    image_sample_explicit_lod,
    image_read,
    image_write,
    read_input,
    write_output,
    read_builtin,
    write_builtin,
    ext_inst,
};

enum class stage_kind : std::uint8_t {
    none = 0,
    vertex = 1,
    fragment = 2,
    compute = 4,
};

enum class stage_role : std::uint8_t {
    input = 0,
    varying = 1,
    output = 2,
};

struct instruction {
    ir_op op = ir_op::nop;
    u32 result_id = 0;
    u32 type_id = 0;
    std::vector<u32> operands;
    std::vector<u32> literals;
};

struct function {
    u32 result_id = 0;
    u32 return_type_id = 0;
    stage_kind stage = stage_kind::none;
    std::vector<u32> parameter_ids;
    std::vector<instruction> body;
};

struct struct_field {
    std::string type;
    std::string name;
};

struct struct_decl {
    std::string name;
    std::vector<struct_field> fields;
};

struct uniform {
    std::string scope_name;
    std::string name;
    std::string type;
    std::string access;
    u32 set = 0;
    u32 binding = 0;
};

struct stage_field {
    std::string name;
    std::string interpolation;
    std::string builtin;
    u32 location = 0;
    bool has_location = false;
};

struct stage_interface {
    stage_role role = stage_role::input;
    std::string type_name;
    std::vector<stage_field> fields;
};

struct artifact_module {
    std::vector<instruction> type_constant_pool;
    std::vector<instruction> global_variables;
    std::vector<function> functions;
    std::vector<struct_decl> structs;
    std::vector<uniform> uniforms;
    std::vector<stage_interface> stage_interfaces;
    u32 next_id = 1;
};

artifact_module read_rtslp_module(u64 program_size, const void* program_source);

} // namespace rtsl

#endif
