-- Compute

#version 450 core

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0) writeonly buffer OutputBuffer {
    float outputValue;
};

#if defined(INPUT_STORAGE_BUFFER)
layout (binding = 1, std430) readonly buffer InputBuffer {
    float values[];
};
#endif

#if defined(INPUT_STORAGE_BUFFER_ARRAY)
layout (binding = 1, std430) readonly buffer InputBuffers {
    float values[];
} fieldBuffers[MEMBER_COUNT];
#endif

#if defined(INPUT_BUFFER_REFERENCE)
layout (binding = 1, buffer_reference, std430) readonly buffer InputBuffer {
    float values[];
} fieldBuffers;
#endif

#if defined(INPUT_BUFFER_REFERENCE_ARRAY)
layout (buffer_reference, std430, buffer_reference_align = 4) buffer InputBuffer {
    float value;
};
layout (binding = 1) uniform UniformBuffer {
    uint64_t fieldsBuffer;
};
#endif

#define IDXS(x,y,z) ((z)*xs*ys + (y)*xs + (x))
#define IDXM(x,y,z,c) ((c)*xs*ys*zs + (z)*xs*ys + (y)*xs + (x))

// Seems like ifdef around pragma does not correctly disable it...
//#ifdef GL_EXT_shader_64bit_indexing
//#pragma shader_64bit_indexing
//#endif
void main() {
    const uint xs = uint(XS);
    const uint ys = uint(YS);
    const uint zs = uint(ZS);
    const uint cs = uint(MEMBER_COUNT);

#if defined(INPUT_STORAGE_BUFFER)
    outputValue = values[IDXM(xs-1u, ys-1u, zs-1u, cs-1u)];
#elif defined(INPUT_STORAGE_BUFFER_ARRAY)
    outputValue = fieldBuffers[cs-1].values[IDXS(xs-1u, ys-1u, zs-1u)];
#elif defined(INPUT_BUFFER_REFERENCE)
    outputValue = fieldBuffers.values[IDXM(xs-1u, ys-1u, zs-1u, cs-1u)];
#elif defined(INPUT_BUFFER_REFERENCE_ARRAY)
    //float inputBuffer = InputBuffer(0);
    //outputValue = inputBuffer;
    InputBuffer fb2 = InputBuffer(fieldsBuffer + 4 * uint64_t(IDXM(xs-1u, ys-1u, zs-1u, cs-1u)));
    outputValue = fb2.value;
#endif
    //outputValue = 9.0f;
}
