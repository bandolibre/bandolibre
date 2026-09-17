#include "bellow_classify.h"

bellow_classify_result_t bellow_classify(bellows_t prev, int32_t value,
                                         int32_t center, int32_t dead, int32_t hyst,
                                         int32_t full_push, int32_t full_pull)
{
  int32_t push_edge = center - dead/2 - hyst/2;
  int32_t pull_edge = center + dead/2 + hyst/2;

  bellow_classify_result_t result;

  switch (prev)
  {
    case BELLOWS_PUSH:
      if (value > pull_edge)              result.direction = BELLOWS_PULL;
      else if (value >= push_edge + hyst) result.direction = BELLOWS_NEUTRAL;
      else                                result.direction = BELLOWS_PUSH;
      break;
    case BELLOWS_PULL:
      if (value < push_edge)              result.direction = BELLOWS_PUSH;
      else if (value <= pull_edge - hyst) result.direction = BELLOWS_NEUTRAL;
      else                                result.direction = BELLOWS_PULL;
      break;
    default:
      if (value < push_edge)              result.direction = BELLOWS_PUSH;
      else if (value > pull_edge)         result.direction = BELLOWS_PULL;
      else                                result.direction = BELLOWS_NEUTRAL;
      break;
  }

  if (result.direction == BELLOWS_PUSH)
  {
    int32_t span = push_edge - full_push;
    int32_t d = value < push_edge ? push_edge - value : 0;
    result.intensity = (span > 0) ? (uint16_t)((uint32_t)d >= (uint32_t)span ? BELLOW_INTENSITY_MAX : ((uint32_t)d * BELLOW_INTENSITY_MAX) / (uint32_t)span) : 0;
  }
  else if (result.direction == BELLOWS_PULL)
  {
    int32_t span = full_pull - pull_edge;
    int32_t d = value > pull_edge ? value - pull_edge : 0;
    result.intensity = (span > 0) ? (uint16_t)((uint32_t)d >= (uint32_t)span ? BELLOW_INTENSITY_MAX : ((uint32_t)d * BELLOW_INTENSITY_MAX) / (uint32_t)span) : 0;
  }
  else
  {
    result.intensity = 0;
  }

  return result;
}
