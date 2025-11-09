/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2022, Christoph Neuhauser
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

#include <Utils/AppSettings.hpp>
#include <Utils/AppLogic.hpp>
#include <Utils/File/FileUtils.hpp>
#include <Utils/File/Logfile.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/Vulkan/Utils/Instance.hpp>
#include <Graphics/Vulkan/Utils/Device.hpp>
#include <Graphics/Vulkan/Utils/Swapchain.hpp>
#include <Graphics/Vulkan/Shader/ShaderManager.hpp>

#include "Tests.hpp"
#include "Math/Math.hpp"

void vulkanErrorCallbackHeadless() {
    std::cerr << "Application callback" << std::endl;
}

int main(int argc, char *argv[]) {
    // Initialize the filesystem utilities.
    sgl::FileUtils::get()->initialize("BufferTest64", argc, argv);

#ifdef DATA_PATH
    if (!sgl::FileUtils::get()->directoryExists("Data") && !sgl::FileUtils::get()->directoryExists("../Data")) {
        sgl::AppSettings::get()->setDataDirectory(DATA_PATH);
    }
#endif
    sgl::AppSettings::get()->initializeDataDirectory();

    // Do not save the settings; this app does not use any UI functionality.
    sgl::AppSettings::get()->setSaveSettings(false);
    // Disable the debug layers, as we know some of the tests without VK_EXT_shader_64bit_indexing are not conformant
    // to the Vulkan standard.
    sgl::AppSettings::get()->getSettings().addKeyValue("window-debugContext", false);

    sgl::AppSettings::get()->setRenderSystem(sgl::RenderSystem::VULKAN);

    sgl::AppSettings::get()->createHeadless();

    sgl::vk::Instance* instance = sgl::AppSettings::get()->getVulkanInstance();
    sgl::AppSettings::get()->getVulkanInstance()->setDebugCallback(&vulkanErrorCallbackHeadless);
    sgl::vk::DeviceFeatures requestedDeviceFeatures{};
    requestedDeviceFeatures.requestedPhysicalDeviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
    requestedDeviceFeatures.requestedPhysicalDeviceFeatures.shaderStorageBufferArrayDynamicIndexing = VK_TRUE; // optionalPhysicalDeviceFeatures?
    requestedDeviceFeatures.requestedPhysicalDeviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE; // optionalPhysicalDeviceFeatures?
    requestedDeviceFeatures.requestedPhysicalDeviceFeatures.shaderInt64 = VK_TRUE; // optionalPhysicalDeviceFeatures?
    // optionalVulkan12Features?
    requestedDeviceFeatures.requestedVulkan12Features.descriptorIndexing = VK_TRUE;
    requestedDeviceFeatures.requestedVulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    requestedDeviceFeatures.requestedVulkan12Features.runtimeDescriptorArray = VK_TRUE;
    requestedDeviceFeatures.requestedVulkan12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    requestedDeviceFeatures.requestedVulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    requestedDeviceFeatures.optionalVulkan12Features.storageBuffer8BitAccess = VK_TRUE;
    std::vector<const char*> requiredDeviceExtensions = {
            VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };
    std::vector<const char*> optionalDeviceExtensions = {
            VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
            VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            // https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_external_memory_host.html
            VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
            // https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_shader_64bit_indexing.html
            VK_EXT_SHADER_64BIT_INDEXING_EXTENSION_NAME
    };

    std::vector<VkPhysicalDevice> physicalDevices = sgl::vk::enumeratePhysicalDevices(instance);
    std::vector<VkPhysicalDevice> suitablePhysicalDevices;
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    for (auto& physicalDevice : physicalDevices) {
        sgl::vk::getPhysicalDeviceProperties(physicalDevice, physicalDeviceProperties);
        if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            continue;
        }
        if (sgl::vk::checkIsPhysicalDeviceSuitable(
                instance, physicalDevice, nullptr, requiredDeviceExtensions, requestedDeviceFeatures, true)) {
            suitablePhysicalDevices.push_back(physicalDevice);
        }
    }
    for (size_t i = 0; i < suitablePhysicalDevices.size(); i++) {
        if (i != 0) {
            std::cout << std::endl << "--------------------------------------------" << std::endl << std::endl;
        }
        auto physicalDevice = suitablePhysicalDevices.at(i);
        auto* device = new sgl::vk::Device;
        device->createDeviceHeadlessFromPhysicalDevice(
                instance, physicalDevice, requiredDeviceExtensions,
                optionalDeviceExtensions, requestedDeviceFeatures, false);

        sgl::AppSettings::get()->setPrimaryDevice(device);
        sgl::AppSettings::get()->initializeSubsystems();

        runTests();

        sgl::AppSettings::get()->releaseDeviceHeadless();
    }
    sgl::AppSettings::get()->release();

    return 0;
}
