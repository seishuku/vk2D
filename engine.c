#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include <stdatomic.h>
#include "math/math.h"
#include "system/system.h"
#include "system/threads.h"
#include "vulkan/vulkan.h"
#include "font/font_6x8.h"
#include "perframe.h"

extern bool isDone;

// Render size
uint32_t renderWidth, renderHeight;

// Vulkan instance handle and context structs
VkInstance vkInstance;
VkuContext_t vkContext;

// extern timing data from system main
extern float fps, fTimeStep, fTime;

// Vulkan swapchain helper struct
VkuSwapchain_t swapchain;

// Per-frame data
PerFrame_t perFrame[VKU_MAX_FRAME_COUNT];

extern vec2 mousePosition;

void RecreateSwapchain(void);

static uint32_t frameIndex=0;

void DrawPixel(int x, int y, float color[3])
{
	if(x<0||y<0||x>=renderWidth||y>=renderHeight)
		return;

	uint8_t *pixel=(uint8_t *)perFrame[frameIndex].fbStagingBuffer.memory->mappedPointer+(4*(y*renderWidth+x));

	if(swapchain.surfaceFormat.format==VK_FORMAT_R8G8B8A8_SRGB)
	{
		*pixel++=(unsigned char)(color[0]*255.0f)&0xFF;
		*pixel++=(unsigned char)(color[1]*255.0f)&0xFF;
		*pixel++=(unsigned char)(color[2]*255.0f)&0xFF;
	}
	else if(swapchain.surfaceFormat.format==VK_FORMAT_B8G8R8A8_SRGB)
	{
		*pixel++=(unsigned char)(color[2]*255.0f)&0xFF;
		*pixel++=(unsigned char)(color[1]*255.0f)&0xFF;
		*pixel++=(unsigned char)(color[0]*255.0f)&0xFF;
	}
}

void DrawLine(int x0, int y0, int x1, int y1, float color[3])
{
	int dx=abs(x1-x0), sx=x0<x1?1:-1;
	int dy=abs(y1-y0), sy=y0<y1?1:-1;
	int err=(dx>dy?dx:-dy)/2, e2;

	while(1)
	{
		DrawPixel(x0, y0, color);

		if(x0==x1&&y0==y1)
			break;

		e2=err;

		if(e2>-dx)
		{
			err-=dy;
			x0+=sx;
		}

		if(e2<dy)
		{
			err+=dx;
			y0+=sy;
		}
	}
}

void DrawCircle(int xc, int yc, int radius, float color[3])
{
	int x=0, y=radius;
	int d=1-radius;

	while(x<=y)
	{
		DrawPixel(xc+x, yc+y, color);
		DrawPixel(xc-x, yc+y, color);
		DrawPixel(xc+x, yc-y, color);
		DrawPixel(xc-x, yc-y, color);
		DrawPixel(xc+y, yc+x, color);
		DrawPixel(xc-y, yc+x, color);
		DrawPixel(xc+y, yc-x, color);
		DrawPixel(xc-y, yc-x, color);

		if(d<0)
			d+=2*x+3;
		else
		{
			d+=2*(x-y)+5;
			y--;
		}

		x++;
	}
}

void DrawFilledCircle(int xc, int yc, int radius, float color[3])
{
	int x=0, y=radius;
	int d=1-radius;

	while(x<=y)
	{
		for(int i=xc-x;i<=xc+x;i++)
		{
			DrawPixel(i, yc+y, color);
			DrawPixel(i, yc-y, color);
		}

		for(int i=xc-y;i<=xc+y;i++)
		{
			DrawPixel(i, yc+x, color);
			DrawPixel(i, yc-x, color);
		}

		if(d<0)
			d+=2*x+3;
		else
		{
			d+=2*(x-y)+5;
			y--;
		}

		x++;
	}
}

void PutChar(int x, int y, char c)
{
	int i, j;

	for(j=0;j<FONT_HEIGHT;j++)
	{
		for(i=0;i<FONT_WIDTH;i++)
		{
			if(fontdata[(c*FONT_HEIGHT)+j]&(0x80>>i))
				DrawPixel(x+i, y+j, (float[]){ 1.0f, 1.0f, 1.0f });
		}
	}
}

void Printf(int x, int y, char *string, ...)
{
	char *ptr, text[4096];
	va_list	ap;
	int sx=x;

	if(string==NULL)
		return;

	va_start(ap, string);
	vsprintf(text, string, ap);
	va_end(ap);

	for(ptr=text;*ptr!='\0';ptr++)
	{
		if(*ptr=='\n'||*ptr=='\r')
		{
			x=sx;
			y+=FONT_HEIGHT;
			continue;
		}
		if(*ptr=='\t')
		{
			x+=FONT_WIDTH*4;
			continue;
		}

		PutChar(x, y, *ptr);
		x+=FONT_WIDTH;
	}
}

uint8_t *buffers[2];

uint8_t FireRed[256]={ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,6,6,6,7,7,8,8,8,9,9,10,10,11,
	11,12,12,13,13,14,14,15,15,16,17,17,18,18,19,20,20,21,22,22,23,23,24,25,25,26,27,27,28,29,30,30,31,32,32,33,34,34,35,36,36,37,38,38,39,40,40,41,41,42,43,43,44,44,45,46,46,47,47,48,48,49,49,50,
	50,51,51,52,52,52,53,53,54,54,54,55,55,55,56,56,56,57,57,57,57,58,58,58,58,59,59,59,59,59,60,60,60,60,60,60,60,61,61,61,61,61,61,61,61,61,61,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,
	62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62,62 };

uint8_t FireGreen[256]={ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,4,4,4,4,4,4,5,5,5,5,5,6,6,6,6,7,7,7,7,8,8,8,8,9,9,9,9,10,10,10,11,11,11,12,12,12,13,13,13,14,14,14,15,15,15,16,16,16,17,17,18,18,18,19,19,19,20,20,
	21,21,21,22,22,23,23,23,24,24,25,25,26,26,26,27,27,28,28,29,29,29,30,30,31,31,32,32,32,33,33,34,34,35,35,35,36,36,37,37,37,38,38,39,39,40,40,40,41,41,42,42,42,43,43,43,44,44,45,45,45,46,46,46,
	47,47,47,48,48,48,49,49,49,50,50,50,50,51,51,51,52,52,52,52,53,53,53,53,54,54,54,54,55,55,55,55,55,56,56,56,56,56,57,57,57,57,57,57,58,58,58,58,58,58,59,59,59,59,59,59,59,59,60,60,60,60,60,60 };

uint8_t FireBlue[256]={ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,
	5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,11,11,11,11,11 };

void Draw(void)
{
	// Clear screen
	memset(perFrame[frameIndex].fbStagingBuffer.memory->mappedPointer, 0, renderWidth*renderHeight*4);

	for(uint32_t i=0;i<renderWidth*4;i++)
		buffers[0][rand()%(renderWidth*4)]=rand()%255;

	for(uint32_t y=2;y<renderHeight-1;y++)
	{
		for(uint32_t x=1;x<renderWidth-1;x++)
		{
			buffers[1][y*renderWidth+x]=(
				buffers[0][(y-1)*renderWidth+(x-1)]+
				buffers[0][(y+1)*renderWidth+(x+0)]+
				buffers[0][(y-2)*renderWidth+(x+1)]+
				buffers[0][(y-2)*renderWidth+(x+0)]
			)/4;
		}
	}

	for(uint32_t y=0;y<renderHeight;y++)
	{
		int flipy=renderHeight-1-y;

		for(uint32_t x=0;x<renderWidth;x++)
			DrawPixel(x, y, (float[]) { (float)FireRed[buffers[1][flipy*renderWidth+x]]/63.0f, (float)FireGreen[buffers[1][flipy*renderWidth+x]]/63.0f, (float)FireBlue[buffers[1][flipy*renderWidth+x]]/63.0f });
	}

	memcpy(buffers[0], buffers[1], renderWidth*renderHeight);

	Printf(0, 0, "FPS: %0.2f\nFrame time: %0.4f", fps, fTimeStep*1000.0f);
}

// Render call from system main event loop
void Render(void)
{
	static uint32_t index=0, lastImageIndex=0, imageIndex=2;

	vkWaitForFences(vkContext.device, 1, &perFrame[index].frameFence, VK_TRUE, UINT64_MAX);

	lastImageIndex=imageIndex;
	VkResult Result=vkAcquireNextImageKHR(vkContext.device, swapchain.swapchain, UINT64_MAX, perFrame[index].presentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);

	if(imageIndex!=((lastImageIndex+1)%swapchain.numImages))
		DBGPRINTF(DEBUG_ERROR, "FRAME OUT OF ORDER: last Index=%d Index=%d\n", lastImageIndex, imageIndex);

	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
	{
		DBGPRINTF(DEBUG_WARNING, "Swapchain out of date or suboptimal... Rebuilding.\n");
		RecreateSwapchain();

		index=(index+1)%swapchain.numImages;

		return;
	}

	// Reset the frame fence and command pool (and thus the command buffer)
	vkResetFences(vkContext.device, 1, &perFrame[index].frameFence);
	vkResetDescriptorPool(vkContext.device, perFrame[index].descriptorPool, 0);
	vkResetCommandPool(vkContext.device, perFrame[index].commandPool, 0);

	// Start recording the commands
	vkBeginCommandBuffer(perFrame[index].commandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	//////
	// Draw actual things \/\/\/\/
	Draw();
	// Draw actual things /\/\/\/\
	//////

	// Copy to swapchain image for presentation
	vkuTransitionLayout(perFrame[index].commandBuffer, swapchain.image[index], 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkBufferImageCopy region=
	{
		0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { renderWidth, renderHeight, 1 }
	};
	vkCmdCopyBufferToImage(perFrame[index].commandBuffer, perFrame[index].fbStagingBuffer.buffer, swapchain.image[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	vkuTransitionLayout(perFrame[index].commandBuffer, swapchain.image[index], 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	vkEndCommandBuffer(perFrame[index].commandBuffer);

	// Submit command queue
	VkSubmitInfo SubmitInfo=
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&perFrame[index].presentCompleteSemaphore,
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&perFrame[index].renderCompleteSemaphore,
		.commandBufferCount=1,
		.pCommandBuffers=&perFrame[index].commandBuffer,
	};

	vkQueueSubmit(vkContext.graphicsQueue, 1, &SubmitInfo, perFrame[index].frameFence);

	// And present it to the screen
	Result=vkQueuePresentKHR(vkContext.graphicsQueue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&perFrame[index].renderCompleteSemaphore,
		.swapchainCount=1,
		.pSwapchains=&swapchain.swapchain,
		.pImageIndices=&imageIndex,
	});

	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		DBGPRINTF(DEBUG_WARNING, "vkQueuePresent out of date or suboptimal...\n");

	index=(index+1)%swapchain.numImages;
	frameIndex=index;
}

bool vkuMemAllocator_Init(VkuContext_t *context);

// Initialization call from system main
bool Init(void)
{
	mousePosition.x=(float)renderWidth/2.0f;
	mousePosition.y=(float)renderHeight/3.0f;

	buffers[0]=(uint8_t *)Zone_Malloc(zone, renderWidth*renderHeight);
	buffers[1]=(uint8_t *)Zone_Malloc(zone, renderWidth*renderHeight);

	memset(buffers[0], 0, renderWidth*renderHeight);
	memset(buffers[1], 0, renderWidth*renderHeight);

	vkuMemAllocator_Init(&vkContext);

	// Other per-frame data
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkuCreateHostBuffer(&vkContext, &perFrame[i].fbStagingBuffer, renderWidth*renderHeight*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		// Create needed fence and semaphores for rendering
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &perFrame[i].frameFence);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].presentCompleteSemaphore);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].renderCompleteSemaphore);

		// Per-frame descriptor pool for main thread
		vkCreateDescriptorPool(vkContext.device, &(VkDescriptorPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
			.poolSizeCount=4,
			.pPoolSizes=(VkDescriptorPoolSize[])
			{
				{
					.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount=1024, // Max number of this descriptor type that can be in each descriptor set?
				},
				{
					.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
					.descriptorCount=1024,
				},
				{
					.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.descriptorCount=1024,
				},
				{
					.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount=1024,
				},
			},
		}, VK_NULL_HANDLE, &perFrame[i].descriptorPool);

		// Create per-frame command pools
		vkCreateCommandPool(vkContext.device, &(VkCommandPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags=0,
			.queueFamilyIndex=vkContext.graphicsQueueIndex,
		}, VK_NULL_HANDLE, &perFrame[i].commandPool);

		// Allocate the command buffers we will be rendering into
		vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool=perFrame[i].commandPool,
			.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount=1,
		}, &perFrame[i].commandBuffer);
	}

	if(!Zone_VerifyHeap(zone))
		exit(-1);

	DBGPRINTF(DEBUG_WARNING, "\nCurrent vulkan zone memory allocations:\n");
	vkuMemAllocator_Print();

	return true;
}

// Rebuild Vulkan swapchain and related data
void RecreateSwapchain(void)
{
	// Wait for the device to complete any pending work
	vkDeviceWaitIdle(vkContext.device);

	// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
	// This is basically just the swapchain, framebuffers, depth buffers, and semaphores (since they can't be just reset like fences).

	vkuDestroySwapchain(&vkContext, &swapchain);
	memset(&swapchain, 0, sizeof(swapchain));

	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkuDestroyBuffer(&vkContext, &perFrame[i].fbStagingBuffer);

		vkDestroyFence(vkContext.device, perFrame[i].frameFence, VK_NULL_HANDLE);
		vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &perFrame[i].frameFence);

		vkDestroySemaphore(vkContext.device, perFrame[i].presentCompleteSemaphore, VK_NULL_HANDLE);
		vkDestroySemaphore(vkContext.device, perFrame[i].renderCompleteSemaphore, VK_NULL_HANDLE);

		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].presentCompleteSemaphore);
		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].renderCompleteSemaphore);
	}

	// Recreate the swapchain, vkuCreateSwapchain will see that there is an existing swapchain and deal accordingly. <--- FIXME: this apparently isn't working???
	vkuCreateSwapchain(&vkContext, &swapchain, VK_TRUE);

	renderWidth=max(2, swapchain.extent.width);
	renderHeight=max(2, swapchain.extent.height);

	for(uint32_t i=0;i<swapchain.numImages;i++)
		vkuCreateHostBuffer(&vkContext, &perFrame[i].fbStagingBuffer, renderWidth*renderHeight*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

// Destroy call from system main
void Destroy(void)
{
	vkDeviceWaitIdle(vkContext.device);

	Zone_Free(zone, buffers[0]);
	Zone_Free(zone, buffers[1]);

	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkuDestroyBuffer(&vkContext, &perFrame[i].fbStagingBuffer);

		// Destroy sync objects
		vkDestroyFence(vkContext.device, perFrame[i].frameFence, VK_NULL_HANDLE);

		vkDestroySemaphore(vkContext.device, perFrame[i].presentCompleteSemaphore, VK_NULL_HANDLE);
		vkDestroySemaphore(vkContext.device, perFrame[i].renderCompleteSemaphore, VK_NULL_HANDLE);

		// Destroy main thread descriptor pools
		vkDestroyDescriptorPool(vkContext.device, perFrame[i].descriptorPool, VK_NULL_HANDLE);

		// Destroy command pools
		vkDestroyCommandPool(vkContext.device, perFrame[i].commandPool, VK_NULL_HANDLE);
	}

	vkuDestroySwapchain(&vkContext, &swapchain);
	//////////

	DBGPRINTF(DEBUG_INFO, "Remaining Vulkan memory blocks:\n");
	vkuMemAllocator_Print();
	vkuMemAllocator_Destroy();
}
