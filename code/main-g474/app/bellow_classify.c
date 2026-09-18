#include "bellow_classify.h"

/* center/dead/hyst/full_push/full_pull are all uint16_t properties (<= 65535),
 * so every int32_t derived from them below (push_edge, pull_edge, span) is
 * comfortably inside float32's 24-bit exact-integer range: comparing or
 * subtracting them against value_f never rounds. The one division (d/span)
 * carries float32's usual ~2^-24 relative error, utterly negligible on a
 * 0..1 output. */
bellow_classify_result_t bellow_classify(bellows_t prev, float value_f,
                                         int32_t center, int32_t dead, int32_t hyst,
                                         int32_t full_push, int32_t full_pull)
{
  int32_t push_edge = center - dead/2 - hyst/2;
  int32_t pull_edge = center + dead/2 + hyst/2;

  bellow_classify_result_t result;

  switch (prev)
  {
    case BELLOWS_PUSH:
      if (value_f > pull_edge)              result.direction = BELLOWS_PULL;
      else if (value_f >= push_edge + hyst) result.direction = BELLOWS_NEUTRAL;
      else                                  result.direction = BELLOWS_PUSH;
      break;
    case BELLOWS_PULL:
      if (value_f < push_edge)              result.direction = BELLOWS_PUSH;
      else if (value_f <= pull_edge - hyst) result.direction = BELLOWS_NEUTRAL;
      else                                  result.direction = BELLOWS_PULL;
      break;
    default:
      if (value_f < push_edge)              result.direction = BELLOWS_PUSH;
      else if (value_f > pull_edge)         result.direction = BELLOWS_PULL;
      else                                  result.direction = BELLOWS_NEUTRAL;
      break;
  }

  if (result.direction == BELLOWS_PUSH)
  {
    int32_t span = push_edge - full_push;
    float d = value_f < push_edge ? (float)push_edge - value_f : 0.0f;
    result.intensity = (span > 0) ? (d >= span ? 1.0f : d / span) : 0.0f;
  }
  else if (result.direction == BELLOWS_PULL)
  {
    int32_t span = full_pull - pull_edge;
    float d = value_f > pull_edge ? value_f - pull_edge : 0.0f;
    result.intensity = (span > 0) ? (d >= span ? 1.0f : d / span) : 0.0f;
  }
  else
  {
    result.intensity = 0.0f;
  }

  return result;
}
