#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "event.h"

// External data from engine.c
extern uint32_t renderWidth, renderHeight;

//////////////////////////////

vec2 mousePosition={ 0.0f, 0.0f };

#ifdef ANDROID
static vec2 oldMousePosition={ 0.0f, 0.0f };
#endif

bool Event_Trigger(EventID ID, void *arg)
{
	if(ID<0||ID>=NUM_EVENTS)
		return false;

	MouseEvent_t *mouseEvent=(MouseEvent_t *)arg;
	uint32_t key=*((uint32_t *)arg);

	switch(ID)
	{
		case EVENT_KEYDOWN:
		{
			switch(key)
			{
				default:		break;
			}

			break;
		}

		case EVENT_KEYUP:
		{
			switch(key)
			{
				default:		break;
			}

			break;
		}

		case EVENT_MOUSEDOWN:
		{
			break;
		}

		case EVENT_MOUSEUP:
		{
			break;
		}

		case EVENT_MOUSEMOVE:
		{
			// Calculate relative movement
			mousePosition=Vec2_Add(mousePosition, (float)mouseEvent->dx, -(float)mouseEvent->dy);

			mousePosition.x=clampf(mousePosition.x, 0.0f, (float)renderWidth);
			mousePosition.y=clampf(mousePosition.y, 0.0f, (float)renderHeight);
			break;
		}

		default:
			return false;
	}

	return true;
}
