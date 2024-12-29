#ifndef SRC_SHADERS_INCLUDE_TYPES_GLSL
#define SRC_SHADERS_INCLUDE_TYPES_GLSL

#ifndef __cplusplus

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint
#define u64 uint64_t

#define i8  int8_t
#define i16 int16_t
#define i32 int
#define i64 int64_t

#define f32 float

#endif // __cplusplus

#endif // SRC_SHADERS_INCLUDE_TYPES_GLSL