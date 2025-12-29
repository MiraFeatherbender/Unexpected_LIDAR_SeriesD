#ifndef RGB_ANIM_ALL_H
#define RGB_ANIM_ALL_H

// One init function per plugin
void rgb_anim_off_init(void);
void rgb_anim_solid_init(void);
void rgb_anim_fire_init(void);
void rgb_anim_heartbeat_init(void);
void rgb_anim_breathe_init(void);

// Aggregator function
void rgb_anim_init_all(void);

#endif // RGB_ANIM_ALL_H