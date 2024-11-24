#ifndef __PERFRAME_H__
#define __PERFRAME_H__

#include "vulkan/vulkan.h"
#include "math/math.h"

typedef struct
{
	VkuBuffer_t fbStagingBuffer;

	// Descriptor pool
	VkDescriptorPool descriptorPool;

	// Command buffer
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;

	// Fences/semaphores
	VkFence frameFence;
	VkSemaphore presentCompleteSemaphore;
	VkSemaphore renderCompleteSemaphore;
} PerFrame_t;

extern PerFrame_t perFrame[VKU_MAX_FRAME_COUNT];

#endif
