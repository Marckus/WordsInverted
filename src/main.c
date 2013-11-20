#include "pebble_fonts.h"
#include "num2words.h"

#define NOINVERTED
#define NOTEST

#ifdef NOTEST
#define MY_UUID { 0xF6, 0x93, 0x61, 0x62, 0xCA, 0xC0, 0x40, 0xEC, 0xBB, 0x9B, 0x9C, 0xBA, 0x8F, 0x7B, 0xD4, 0xE6 }
#else
#define MY_UUID { 0xDB, 0x00, 0x72, 0x43, 0xD4, 0xCA, 0x4A, 0x0B, 0xA5, 0xF0, 0xA6, 0x2C, 0xAB, 0x89, 0xC5, 0xC8 }
#endif
#define TIME_SLOT_ANIMATION_DURATION 700
#define NUM_LAYERS 4

enum layer_names {
	HOURS,
	MINUTES,
	TENS,
	DATE,
};

PBL_APP_INFO(MY_UUID,
#ifdef TEST
             "Words + Date test", "MikeBikeMusic",
#else
			 #ifdef INVERTED
             "Words + Date Inverted", "MikeBikeMusic",
			 #define BGCOLOR GColorWhite
			 #define FGCOLOR GColorBlack
			 #else
             "Words + Date", "MikeBikeMusic",
			 #define BGCOLOR GColorBlack
			 #define FGCOLOR GColorWhite
			 #endif
#endif
             1, 0, /* App version */
             RESOURCE_ID_MENU_ICON_WHITE,
             APP_INFO_WATCH_FACE);

Window window;
TextLayer text_date_layer;

typedef struct CommonWordsData {
  TextLayer label;
  PropertyAnimation in_animation;
  PropertyAnimation out_animation;
  void (*update) (PblTm *t, char *words);
  char buffer[BUFFER_SIZE];
  GFont font;
} CommonWordsData;

static GFont font_nevis;
static GFont font_gothic;

static struct CommonWordsData layers[NUM_LAYERS] =
{{ .update = &fuzzy_hours_to_words },
 { .update = &fuzzy_sminutes_to_words },
 { .update = &fuzzy_minutes_to_words },
 { .update = &fuzzy_dates_to_words },
};

static bool narrow_the_tens[60] = {
	false, false, false, false, false, false, false, false, false, false, 
	false, false, false, true, true, false, false, false, true, true, 
	false, false, false, false, false, false, false, false, false, false, 
	false, false, false, false, false, false, false, false, false, false, 
	false, false, false, false, false, false, false, false, false, false, 
	false, false, false, false, false, false, false, false, false, false, 
};

static bool narrow_the_day[7][31] = {
	{false, false, false, false, false, false, false, false, false, false, 
	 false, false, false, false, false, false, false, false, false, false,
	 false, false, false, false, false, false, false, false, false, false, false},
	{false, false, false, false, false, false, false, false, false, false, 
	 false, false, false, false, false, false, false, false, false, true,
	 false, false, false, true,  false, true,  false, false, true,  true, false}, 
	{false, false, false, false, false, false, false, false, false, false, 
	 false, false, false, false, false, false, false, false, false, false,
	 false, false, false, false, false, false, false, false, false, false, false},
	{true,  true,  true,  true,  true,  true,  true,  true,  true,  true, 
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  
	 true,  true,  true,  true,  true,  true,  true,  true,  true,  true, true}, 
	{false, false, false, false, false, false, false, false, false, false, 
	 false, false, false, false, false, false, false, false, false, false,
	 false, false, false, false, false, false, false, false, false, false, false},
	{false, false, false, false, false, false, false, false, false, false, 
	 false, false, false, false, false, false, false, false, false, false,
	 false, false, false, false, false, false, false, false, false, false, false},
	{false, false, false, false, false, false, false, false, false, false, 
	 false, false, false, false, false, false, false, false, false, false,
	 false, false, false, false, false, false, false, false, false, false, false}
};

static bool tens_update[60] = {
	true, true, false, false, false, false, false, false, false, false, 
	true, true, true, true, true, true, true, true, true, true, 
	true, false, false, false, false, false, false, false, false, false, 
	true, false, false, false, false, false, false, false, false, false, 
	true, false, false, false, false, false, false, false, false, false, 
	true, false, false, false, false, false, false, false, false, false, 
};

static bool minutes_update[60] = {
	true, true, true, true, true, true, true, true, true, true, 
	true, false, false, false, false, false, false, true, true, false, 
	false, true, true, true, true, true, true, true, true, true, 
	true, true, true, true, true, true, true, true, true, true, 
	true, true, true, true, true, true, true, true, true, true, 
	true, true, true, true, true, true, true, true, true, true, 
};

void slide_out(PropertyAnimation *animation, CommonWordsData *layer) {
  GRect from_frame = layer_get_frame(&layer->label.layer);

  GRect to_frame = GRect(-window.layer.frame.size.w, from_frame.origin.y,
                          window.layer.frame.size.w, from_frame.size.h);

  property_animation_init_layer_frame(animation, &layer->label.layer, NULL,
                                        &to_frame);
  animation_set_duration(&animation->animation, TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(&animation->animation, AnimationCurveEaseIn);
}

void slide_in(PropertyAnimation *animation, CommonWordsData *layer) {
  GRect to_frame = layer_get_frame(&layer->label.layer);
  GRect from_frame = GRect(2*window.layer.frame.size.w, to_frame.origin.y,
                            window.layer.frame.size.w, to_frame.size.h);

  layer_set_frame(&layer->label.layer, from_frame);
  text_layer_set_text(&layer->label, layer->buffer);
  property_animation_init_layer_frame(animation, &layer->label.layer, NULL, &to_frame);
  animation_set_duration(&animation->animation, TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(&animation->animation, AnimationCurveEaseOut);
}

void slide_out_animation_stopped(Animation *slide_out_animation, bool finished,
                                  void *context) {
  CommonWordsData *layer = (CommonWordsData *)context;
  layer->label.layer.frame.origin.x = 0;
  PblTm t;
  get_time(&t);
#ifdef TEST
	if (t.tm_sec != 0) {
		t.tm_wday = (t.tm_min % 7);
		t.tm_mday = t.tm_sec / 2 + 1;
  	//	text_layer_set_font(&layer->label, font_gothic);
		layers[DATE].font = narrow_the_day[t.tm_wday][t.tm_mday - 1] ? font_nevis : font_gothic;
	}
#endif
	  text_layer_set_font(&layer->label, layer->font);
  layer->update(&t, layer->buffer);
  slide_in(&layer->in_animation, layer);
  animation_schedule(&layer->in_animation.animation);
}

void update_layer(CommonWordsData *layer) {
  slide_out(&layer->out_animation, layer);
  animation_set_handlers(&layer->out_animation.animation, (AnimationHandlers){
    .stopped = (AnimationStoppedHandler)slide_out_animation_stopped
  }, (void *) layer);
  animation_schedule(&layer->out_animation.animation);
}

static void handle_minute_tick(AppContextRef app_ctx, PebbleTickEvent* e) {
  PblTm *t = e->tick_time;
#ifdef TEST
	if ((t->tm_sec & 1) == 1) {
		e->units_changed |= DAY_UNIT;
	}
#endif
  if((e->units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
    if (minutes_update[t->tm_min]) {
      update_layer(&layers[MINUTES]);
    }
	if (tens_update[t->tm_min]) {
		layers[TENS].font = narrow_the_tens[t->tm_min] ? font_nevis : font_gothic;
		update_layer(&layers[TENS]);
    }
  }
  if ((e->units_changed & HOUR_UNIT) == HOUR_UNIT ||
        ((t->tm_hour == 00 || t->tm_hour == 12) && t->tm_min == 01)) {
    update_layer(&layers[HOURS]);
  }
  if ((e->units_changed & DAY_UNIT) == DAY_UNIT) {
		layers[DATE].font = narrow_the_day[t->tm_wday][t->tm_mday - 1] ? font_nevis : font_gothic;
	  	update_layer(&layers[DATE]);
  }
}

void init_layer(CommonWordsData *layer, GRect rect, GFont font) {
  text_layer_init(&layer->label, rect);
  text_layer_set_background_color(&layer->label, BGCOLOR);
  text_layer_set_text_color(&layer->label, FGCOLOR);
  text_layer_set_font(&layer->label, font);
  layer->font = font;
  layer_add_child(&window.layer, &layer->label.layer);
}

void show_layer(CommonWordsData *layer, PblTm *t) {
    text_layer_set_font(&layer->label, layer->font);
    layer->update(t, layer->buffer);
    slide_in(&layer->in_animation, layer);
    animation_schedule(&layer->in_animation.animation);
}

void handle_init(AppContextRef ctx) {
  (void)ctx;

  window_init(&window, "Words + Date Inverted");
  const bool animated = true;
  window_stack_push(&window, animated);
  window_set_background_color(&window, BGCOLOR);
  resource_init_current_app(&APP_RESOURCES);

	font_nevis = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_GOTHAMXNARROW_BOLD_42));
	font_gothic = fonts_get_system_font(FONT_KEY_GOTHAM_42_BOLD);

// single digits
  init_layer(&layers[MINUTES],GRect(0, 42*2-8, window.layer.frame.size.w, 42+8), font_gothic);

// 00 minutes
  init_layer(&layers[TENS], GRect(0, 42-6, window.layer.frame.size.w, 42+8), font_gothic);

//hours
  init_layer(&layers[HOURS], GRect(0, 0-8, window.layer.frame.size.w, 42+6), font_gothic);

//Date
  init_layer(&layers[DATE], GRect(0, 42*3-2, window.layer.frame.size.w, 42+4), font_gothic);

//show your face
  PblTm t;
  get_time(&t);

	if (narrow_the_tens[t.tm_min])
		layers[TENS].font = font_nevis;

	if (narrow_the_day[t.tm_wday][t.tm_mday - 1])
		layers[DATE].font = font_nevis;

  for (int i = 0; i < NUM_LAYERS; ++i)
  {
    text_layer_set_font(&layers[i].label, layers[i].font);
    layers[i].update(&t, layers[i].buffer);
    slide_in(&layers[i].in_animation, &layers[i]);
    animation_schedule(&layers[i].in_animation.animation);
  }

}

static void handle_deinit(AppContextRef ctx)
{
	(void) ctx;
	
	fonts_unload_custom_font(font_nevis);
}

void pbl_main(void *params) {
 PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
#ifdef TEST
      .tick_units = SECOND_UNIT
#else
      .tick_units = MINUTE_UNIT
#endif
    }

  };
  app_event_loop(params, &handlers);
}