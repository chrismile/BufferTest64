# BufferTest64

A program for testing Vulkan buffers with sizes larger than 4GiB. It attempts to use the following extensions:
- [VK_EXT_shader_64bit_indexing](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_shader_64bit_indexing.html)
- [GL_EXT_shader_64bit_indexing](https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GL_EXT_shader_64bit_indexing.txt)
- [SPV_EXT_shader_64bit_indexing](https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_64bit_indexing.html)

It attempts to allocate buffers larger than 4GiB with and without these extensions and tries to read back the last
entry from these buffers in a compute shader.
