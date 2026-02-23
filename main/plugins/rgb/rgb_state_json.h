#ifndef RGB_STATE_JSON_H
#define RGB_STATE_JSON_H

#include <stdint.h>
#include "rest_context.h"
#include "rgb_anim.h"

void rgb_state_json_fill(rest_json_request_t *req,
                         uint8_t plugin_id,
                         hsv_color_t hsv,
                         uint8_t brightness);

#endif // RGB_STATE_JSON_H
