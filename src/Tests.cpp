/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2023, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <cstring>

#include <Math/Math.hpp>
#include <Utils/AppSettings.hpp>
#include <Graphics/Vulkan/Utils/SyncObjects.hpp>
#include <Graphics/Vulkan/Buffers/Buffer.hpp>
#include <Graphics/Vulkan/Render/Renderer.hpp>
#include <Graphics/Vulkan/Render/Passes/Pass.hpp>
#include <ImGui/Widgets/NumberFormatting.hpp>

#include "Tests.hpp"

#include "Graphics/Vulkan/Render/ComputePipeline.hpp"

enum class TestMode {
    STORAGE_BUFFER = 0,
    STORAGE_BUFFER_ARRAY = 1,
    BUFFER_REFERENCE = 2,
    BUFFER_REFERENCE_ARRAY = 3,
    STORAGE_BUFFER_64_BIT = 4,
    BUFFER_REFERENCE_64_BIT = 5,
    BUFFER_REFERENCE_ARRAY_64_BIT = 6,
};
const int NUM_TESTS = 7;
const char* TEST_MODE_NAMES[] = {
    "Storage buffer",
    "Storage buffer array",
    "Buffer reference",
    "Buffer reference array",
    "Storage buffer (64-bit)",
    "Buffer reference (64-bit)",
    "Buffer reference array (64-bit)",
};
bool TEST_MODE_USES_ARRAY[] = {
    false,
    true,
    false,
    false,
    false,
    false,
    false,
};

enum class TestDataType {
    FLOAT = 0,
    UINT8 = 1,
};
const int NUM_TEST_DATA_TYPES = 2;
const char* TEST_DATA_TYPE_NAMES[] = {
    "float",
    "uint8_t",
};


class BufferTestComputePass : public sgl::vk::ComputePass {
public:
    explicit BufferTestComputePass(sgl::vk::Renderer* renderer, uint32_t xs, uint32_t ys, uint32_t zs, uint32_t cs);
    void setTestMode(TestMode _testMode);
    void setDataType(TestDataType _dataType);
    void setFieldsBuffer(const sgl::vk::BufferPtr& _fieldsBuffer);
    void setFieldBuffers(const std::vector<sgl::vk::BufferPtr>& _fieldBuffers);
    inline sgl::vk::BufferPtr getOutputBuffer() { return outputBuffer; }

protected:
    void loadShader() override;
    void setComputePipelineInfo(sgl::vk::ComputePipelineInfo& pipelineInfo) override;
    void createComputeData(sgl::vk::Renderer* renderer, sgl::vk::ComputePipelinePtr& computePipeline) override;
    void _render() override;

private:
    uint32_t xs, ys, zs, cs;
    sgl::vk::BufferPtr uniformBuffer;
    sgl::vk::BufferPtr outputBuffer;
    TestMode testMode = TestMode::STORAGE_BUFFER;
    TestDataType dataType = TestDataType::FLOAT;
    sgl::vk::BufferPtr fieldsBuffer;
    std::vector<sgl::vk::BufferPtr> fieldBuffers;
};

BufferTestComputePass::BufferTestComputePass(
        sgl::vk::Renderer* renderer, uint32_t xs, uint32_t ys, uint32_t zs, uint32_t cs)
        : ComputePass(renderer), xs(xs), ys(ys), zs(zs), cs(cs) {
    float data = 0.0f;
    outputBuffer = std::make_shared<sgl::vk::Buffer>(
            device, sizeof(float), &data,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);
    uniformBuffer = std::make_shared<sgl::vk::Buffer>(
            device, sizeof(uint64_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);
}

void BufferTestComputePass::setTestMode(TestMode _testMode) {
    testMode = _testMode;
    setShaderDirty();
}

void BufferTestComputePass::setDataType(TestDataType _dataType) {
    dataType = _dataType;
    setShaderDirty();
}

void BufferTestComputePass::setFieldsBuffer(const sgl::vk::BufferPtr& _fieldsBuffer) {
    fieldsBuffer = _fieldsBuffer;
    setDataDirty();
}

void BufferTestComputePass::setFieldBuffers(const std::vector<sgl::vk::BufferPtr>& _fieldBuffers) {
    fieldBuffers = _fieldBuffers;
    setDataDirty();
}

void BufferTestComputePass::loadShader() {
    sgl::vk::ShaderManager->invalidateShaderCache();
    std::map<std::string, std::string> preprocessorDefines;
    preprocessorDefines.insert(std::make_pair("XS", std::to_string(xs)));
    preprocessorDefines.insert(std::make_pair("YS", std::to_string(ys)));
    preprocessorDefines.insert(std::make_pair("ZS", std::to_string(zs)));
    preprocessorDefines.insert(std::make_pair("MEMBER_COUNT", std::to_string(cs)));
    if (testMode == TestMode::STORAGE_BUFFER || testMode == TestMode::STORAGE_BUFFER_64_BIT) {
        preprocessorDefines.insert(std::make_pair("INPUT_STORAGE_BUFFER", ""));
    } else if (testMode == TestMode::STORAGE_BUFFER_ARRAY) {
        preprocessorDefines.insert(std::make_pair("INPUT_STORAGE_BUFFER_ARRAY", ""));
    } else if (testMode == TestMode::BUFFER_REFERENCE || testMode == TestMode::BUFFER_REFERENCE_64_BIT) {
        preprocessorDefines.insert(std::make_pair("INPUT_BUFFER_REFERENCE", ""));
    } else if (testMode == TestMode::BUFFER_REFERENCE_ARRAY || testMode == TestMode::BUFFER_REFERENCE_ARRAY_64_BIT) {
        preprocessorDefines.insert(std::make_pair("INPUT_BUFFER_REFERENCE_ARRAY", ""));
    }
    std::vector<std::string> extensions;
    if (testMode == TestMode::STORAGE_BUFFER_64_BIT || testMode == TestMode::BUFFER_REFERENCE_64_BIT
            || testMode == TestMode::BUFFER_REFERENCE_ARRAY_64_BIT) {
        // https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GL_EXT_shader_64bit_indexing.txt
        // https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_shader_64bit_indexing.html
        extensions.emplace_back("GL_EXT_shader_64bit_indexing");
        preprocessorDefines.insert(std::make_pair("USE_64_BIT_INDEXING", ""));
    }
    if (dataType == TestDataType::FLOAT) {
        preprocessorDefines.insert(std::make_pair("DATA_TYPE", "float"));
        preprocessorDefines.insert(std::make_pair("DATA_TYPE_SIZE", "4"));
    } else if (dataType == TestDataType::UINT8) {
        preprocessorDefines.insert(std::make_pair("DATA_TYPE", "uint8_t"));
        preprocessorDefines.insert(std::make_pair("DATA_TYPE_SIZE", "1"));
        extensions.emplace_back("GL_EXT_shader_8bit_storage");
        extensions.emplace_back("GL_EXT_shader_explicit_arithmetic_types_int8");
    }
    if (!extensions.empty()) {
        std::string extensionsString;
        for (size_t i = 0; i < extensions.size(); ++i) {
            if (i != 0) {
                extensionsString += ";";
            }
            extensionsString += extensions[i];
        }
        preprocessorDefines.insert(std::make_pair("__extensions", extensionsString));
    }
    shaderStages = sgl::vk::ShaderManager->getShaderStages({ "TestBuffer.Compute" }, preprocessorDefines);
}

void BufferTestComputePass::setComputePipelineInfo(sgl::vk::ComputePipelineInfo& pipelineInfo) {
    if (testMode == TestMode::STORAGE_BUFFER_64_BIT || testMode == TestMode::BUFFER_REFERENCE_64_BIT
            || testMode == TestMode::BUFFER_REFERENCE_ARRAY_64_BIT) {
        // https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_shader_64bit_indexing.html
        pipelineInfo.setUse64BitIndexing(true);
    }
}

void BufferTestComputePass::createComputeData(sgl::vk::Renderer* renderer, sgl::vk::ComputePipelinePtr& computePipeline) {
    computeData = std::make_shared<sgl::vk::ComputeData>(renderer, computePipeline);
    computeData->setStaticBuffer(outputBuffer, "OutputBuffer");
    if (testMode == TestMode::STORAGE_BUFFER || testMode == TestMode::STORAGE_BUFFER_64_BIT) {
        computeData->setStaticBuffer(fieldsBuffer, "InputBuffer");
    } else if (testMode == TestMode::STORAGE_BUFFER_ARRAY) {
        computeData->setStaticBufferArray(fieldBuffers, "InputBuffers");
    } else if (testMode == TestMode::BUFFER_REFERENCE || testMode == TestMode::BUFFER_REFERENCE_64_BIT) {
        computeData->setStaticBuffer(fieldsBuffer, "InputBuffer");
    } else if (testMode == TestMode::BUFFER_REFERENCE_ARRAY || testMode == TestMode::BUFFER_REFERENCE_ARRAY_64_BIT) {
        computeData->setStaticBuffer(uniformBuffer, "UniformBuffer");
    }
}

void BufferTestComputePass::_render() {
    if (testMode == TestMode::BUFFER_REFERENCE_ARRAY) {
        uint64_t val = fieldsBuffer->getVkDeviceAddress();
        uniformBuffer->updateData(sizeof(uint64_t), &val, renderer->getVkCommandBuffer());
        renderer->insertBufferMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                uniformBuffer);
        computeData->setStaticBuffer(uniformBuffer, "UniformBuffer");
    }
    renderer->dispatch(computeData, 1, 1, 1);
}

void runTest(uint32_t xs, uint32_t ys, uint32_t zs, uint32_t cs, TestDataType testDataType, bool useHostAllocation) {
    sgl::vk::Device* device = sgl::AppSettings::get()->getPrimaryDevice();
    std::cout << std::endl;

    size_t numEntries3D = size_t(xs) * size_t(ys) * size_t(zs);
    size_t numEntries = size_t(xs) * size_t(ys) * size_t(zs) * size_t(cs);
    size_t sizeInBytes3D = sizeof(float) * numEntries3D;
    size_t sizeInBytes = sizeof(float) * numEntries;
    if (testDataType == TestDataType::UINT8) {
        cs *= 4;
        numEntries *= 4;
    }

    auto outputStagingBuffer = std::make_shared<sgl::vk::Buffer>(
            device, sizeof(float),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);

    auto fence = std::make_shared<sgl::vk::Fence>(device);
    sgl::vk::CommandPoolType commandPoolType{};
    commandPoolType.queueFamilyIndex = device->getComputeQueueIndex();
    commandPoolType.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer = device->allocateCommandBuffer(commandPoolType, &commandPool);

    std::cout << "Allocation size " << sgl::getNiceMemoryString(sizeInBytes, 2) << ", type " << TEST_DATA_TYPE_NAMES[int(testDataType)];
    if (useHostAllocation) {
        std::cout << ", host allocation" << std::endl;
    } else {
        std::cout << ", device allocation" << std::endl;
    }

    auto* renderer = new sgl::vk::Renderer(device, 2000);
    for (int i = 0; i < NUM_TESTS; i++) {
        if (testDataType == TestDataType::UINT8 && TEST_MODE_USES_ARRAY[i]) {
            continue;
        }
        if (!device->getShader64BitIndexingFeaturesEXT().shader64BitIndexing && i >= int(TestMode::STORAGE_BUFFER_64_BIT)) {
            break;
        }
        std::cout << "Starting test case '" << TEST_MODE_NAMES[i] << "'..." << std::endl;
        renderer->setCustomCommandBuffer(commandBuffer, false);
        renderer->beginCommandBuffer();

        auto testMode = TestMode(i);
        auto pass0 = std::make_shared<BufferTestComputePass>(renderer, xs, ys, zs, cs);
        pass0->setTestMode(testMode);
        pass0->setDataType(testDataType);
        if (TEST_MODE_USES_ARRAY[i]) {
            auto* data = new float[numEntries3D];
            memset(data, 0, sizeInBytes3D);
            for (size_t i = 0; i < numEntries3D - 1; i++) {
                data[i] = 7.0f;
            }
            sgl::vk::BufferPtr fieldBuffer0(new sgl::vk::Buffer(
                    device, sizeInBytes3D, data,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY));
            data[numEntries3D - 1] = 42.0f;
            sgl::vk::BufferPtr fieldBuffer1(new sgl::vk::Buffer(
                    device, sizeInBytes3D, data,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY));
            delete[] data;
            std::vector<sgl::vk::BufferPtr> fieldBuffers;
            for (uint32_t j = 0; j < cs - 1; j++) {
                fieldBuffers.push_back(fieldBuffer0);
            }
            fieldBuffers.push_back(fieldBuffer1);
            pass0->setFieldBuffers(fieldBuffers);
        } else {
            sgl::vk::BufferPtr fieldsBuffer;
            void* hostPtr = nullptr;
            if (useHostAllocation) {
                // Check claims from https://community.khronos.org/t/memory-import-size-truncated-on-windows/111813.
                // https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_external_memory_host.html
                fieldsBuffer = std::make_shared<sgl::vk::Buffer>(device);
                hostPtr = fieldsBuffer->allocateFromNewHostPointer(
                        sizeInBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            } else {
                hostPtr = std::malloc(numEntries * sizeof(float));
            }
            if (testDataType == TestDataType::FLOAT) {
                auto* data = static_cast<float*>(hostPtr);
                memset(data, 0, sizeInBytes);
                for (size_t j = 0; j < numEntries - 1; j++) {
                    data[j] = float(j);
                }
                data[numEntries - 1] = 42.0f;
            } else if (testDataType == TestDataType::UINT8) {
                auto* data = static_cast<uint8_t*>(hostPtr);
                memset(data, 0, sizeInBytes);
                for (size_t j = 0; j < numEntries - 1; j++) {
                    data[j] = 7;
                }
                data[numEntries - 1] = 42;
            }
            if (!useHostAllocation) {
                fieldsBuffer = std::make_shared<sgl::vk::Buffer>(
                        device, sizeInBytes, hostPtr,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VMA_MEMORY_USAGE_GPU_ONLY);
                free(hostPtr);
            }
            pass0->setFieldsBuffer(fieldsBuffer);
        }
        auto outputBuffer = pass0->getOutputBuffer();
        pass0->render();

        renderer->insertBufferMemoryBarrier(
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                outputBuffer);
        outputBuffer->copyDataTo(outputStagingBuffer, renderer->getVkCommandBuffer());

        renderer->endCommandBuffer();
        renderer->submitToQueue({}, {}, fence, VK_PIPELINE_STAGE_TRANSFER_BIT);
        renderer->resetCustomCommandBuffer();
        fence->wait();
        fence->reset();

        float outputValue = 0.0f;
        if (testDataType == TestDataType::FLOAT) {
            auto* outputValuePtr = static_cast<float*>(outputStagingBuffer->mapMemory());
            outputValue = *outputValuePtr;
        } else if (testDataType == TestDataType::UINT8) {
            auto* outputValuePtr = static_cast<uint8_t*>(outputStagingBuffer->mapMemory());
            outputValue = *outputValuePtr;
        }
        outputStagingBuffer->unmapMemory();
        std::string testResult;
        if (outputValue == 42) {
            testResult = "Passed";
        } else {
            testResult = "Failed";
        }
        std::cout << "Test case '" << TEST_MODE_NAMES[i] << "': " << testResult << " (" << outputValue << ")" << std::endl;
    }

    device->freeCommandBuffer(commandPool, commandBuffer);
    delete renderer;
}

void runTests() {
    sgl::vk::Device* device = sgl::AppSettings::get()->getPrimaryDevice();
    std::cout << "Device name: " << device->getDeviceName() << std::endl;
    if (device->getPhysicalDeviceProperties().apiVersion >= VK_API_VERSION_1_1) {
        std::cout << "Device driver name: " << device->getDeviceDriverName() << std::endl;
        std::cout << "Device driver info: " << device->getDeviceDriverInfo() << std::endl;
        std::cout << "Device driver ID: " << device->getDeviceDriverId() << std::endl;
    }
    std::cout << "Max memory allocations: "
            << sgl::getNiceMemoryStringDifference(device->getLimits().maxMemoryAllocationCount, 2, true) << std::endl;
    std::cout << "Max storage buffer range: "
            << sgl::getNiceMemoryStringDifference(device->getLimits().maxStorageBufferRange, 2, true) << std::endl;
    std::cout << "Max memory allocation size: "
            << sgl::getNiceMemoryStringDifference(device->getPhysicalDeviceVulkan11Properties().maxMemoryAllocationSize, 2, true) << std::endl;
    std::cout << "Supports shader 64-bit indexing: " << (device->getShader64BitIndexingFeaturesEXT().shader64BitIndexing ? "Yes" : "No") << std::endl;
    std::cout << "alignof(std::max_align_t): " << alignof(std::max_align_t) << std::endl;
    std::cout << "Min imported host pointer alignment: " << device->getMinImportedHostPointerAlignment() << std::endl;

    std::vector<std::string> flagsStringMap = {
            "device local",  // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            "host visible",  // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            "host coherent", // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            "host cached"    // VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    };
    const VkPhysicalDeviceMemoryProperties& deviceMemoryProperties = device->getMemoryProperties();
    for (uint32_t heapIdx = 0; heapIdx < deviceMemoryProperties.memoryHeapCount; heapIdx++) {
        VkMemoryPropertyFlagBits typeFlags{};
        for (uint32_t memoryTypeIdx = 0; memoryTypeIdx < deviceMemoryProperties.memoryTypeCount; memoryTypeIdx++) {
            if (deviceMemoryProperties.memoryTypes[memoryTypeIdx].heapIndex == heapIdx) {
                typeFlags = VkMemoryPropertyFlagBits(typeFlags | deviceMemoryProperties.memoryTypes[memoryTypeIdx].propertyFlags);
            }
        }
        std::string memoryHeapInfo;
        if (typeFlags != 0) {
            memoryHeapInfo = " (";
            typeFlags = VkMemoryPropertyFlagBits(typeFlags & 0xF);
            auto numEntries = int(sgl::popcount(uint32_t(typeFlags)));
            int entryIdx = 0;
            for (int i = 0; i < 4; i++) {
                auto flag = VkMemoryPropertyFlagBits(1 << i);
                if ((typeFlags & flag) != 0) {
                    memoryHeapInfo += flagsStringMap[i];
                    if (entryIdx != numEntries - 1) {
                        memoryHeapInfo += ", ";
                    }
                    entryIdx++;
                }
            }
            memoryHeapInfo += ")";
        }
        bool hasTypeDeviceLocal = (typeFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        bool isHeapDeviceLocal = (deviceMemoryProperties.memoryHeaps[heapIdx].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
        if (hasTypeDeviceLocal != isHeapDeviceLocal) {
            sgl::Logfile::get()->writeError("Encountered memory heap with mismatching heap and type flags.");
        }
        std::cout
                << "Memory heap #" << heapIdx << ": "
                << sgl::getNiceMemoryStringDifference(deviceMemoryProperties.memoryHeaps[heapIdx].size, 2, true)
                << memoryHeapInfo << std::endl;
    }

    std::vector<std::vector<uint32_t>> allocationSizes = {
            { 512, 512, 512, 5 }, // 2.5GiB
            { 512, 512, 512, 10 }, // 5GiB
    };

    for (const auto& allocSize : allocationSizes) {
        for (int testDataTypeIdx = 0; testDataTypeIdx < NUM_TEST_DATA_TYPES; testDataTypeIdx++) {
            auto testDataType = TestDataType(testDataTypeIdx);
            if (testDataType == TestDataType::UINT8
                    && !device->getPhysicalDeviceVulkan12Features().storageBuffer8BitAccess) {
                continue;
            }
            for (int useHostAllocation = 0; useHostAllocation <= 1; useHostAllocation++) {
                if (testDataType == TestDataType::UINT8 && useHostAllocation) {
                    continue;
                }
                runTest(
                        allocSize[0], allocSize[1], allocSize[2], allocSize[3],
                        testDataType, bool(useHostAllocation));
            }
        }
    }
}
