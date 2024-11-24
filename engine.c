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

	*pixel++=(unsigned char)(color[2]*255.0f)&0xFF;
	*pixel++=(unsigned char)(color[1]*255.0f)&0xFF;
	*pixel++=(unsigned char)(color[0]*255.0f)&0xFF;
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

int16_t *wavemap[2], curWavemap=0, prevWavemap=1;

void start_drop()
{
	uint32_t x=(renderWidth*RandFloat());
	uint32_t y=(renderHeight*RandFloat());

	if(x<1||x>renderWidth-1)
		return;
	if(y<1||y>renderHeight-1)
		return;

	wavemap[curWavemap][y*renderWidth+x]=30000;
}

void Draw(void)
{
	// Clear screen
	memset(perFrame[frameIndex].fbStagingBuffer.memory->mappedPointer, 0, renderWidth*renderHeight*4);

	start_drop();

	for(uint32_t y=1;y<renderHeight-1;y++)
	{
		for(uint32_t x=1;x<renderHeight-1;x++)
		{
			int16_t height=((
				wavemap[prevWavemap][(y)*renderWidth+(x-1)]+
				wavemap[prevWavemap][(y)*renderWidth+(x+1)]+
				wavemap[prevWavemap][(y+1)*renderWidth+(x)]+
				wavemap[prevWavemap][(y-1)*renderWidth+(x)]
				)>>1)-wavemap[curWavemap][y*renderWidth+x];
			wavemap[curWavemap][y*renderWidth+x]=(height*254)>>8;

			float pixel=((wavemap[curWavemap][y*renderWidth+x]+32768)>>8)/255.0f;
			DrawPixel(x, y, (float[]) { pixel, pixel, pixel });
		}
	}

	short temp=curWavemap;
	curWavemap=prevWavemap;
	prevWavemap=temp;

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
	vkuTransitionLayout(perFrame[index].commandBuffer, swapchain.image[index], 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkBufferImageCopy region=
	{
		0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { renderWidth, renderHeight, 1 }
	};
	vkCmdCopyBufferToImage(perFrame[index].commandBuffer, perFrame[index].fbStagingBuffer.buffer, swapchain.image[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	vkuTransitionLayout(perFrame[index].commandBuffer, swapchain.image[index], 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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

	wavemap[0]=(int16_t *)Zone_Malloc(zone, sizeof(int16_t)*renderWidth*renderHeight);
	wavemap[1]=(int16_t *)Zone_Malloc(zone, sizeof(int16_t)*renderWidth*renderHeight);

	memset(wavemap[0], 0, sizeof(int16_t)*renderWidth*renderHeight);
	memset(wavemap[1], 0, sizeof(int16_t)*renderWidth*renderHeight);

	curWavemap=0;
	prevWavemap=1;

	vkuMemAllocator_Init(&vkContext);

	for(uint32_t i=0;i<swapchain.numImages;i++)
		vkuCreateHostBuffer(&vkContext, &perFrame[i].fbStagingBuffer, renderWidth*renderHeight*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	// Other per-frame data
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
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

	Zone_Free(zone, wavemap[0]);
	Zone_Free(zone, wavemap[1]);

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
