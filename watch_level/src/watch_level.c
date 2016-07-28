/*
 ** watch_level.c
 * Creates a watch level app
 */

#include <pebble.h>
#include <math.h> 

/* **** Constants **** */
#define P_WIDTH 144
#define P_LENGTH 168

#define BOX_SIZE_X 12
#define BOX_SIZE_Y 12
#define SMALL_BUBBLE_X 8
#define SMALL_BUBBLE_Y 20

#define DIAMETER 94
#define PRACTICAL_RADIUS 41 // Radius without border
#define MARGIN 37 // Margin of the surfact from the left
#define LINE_LEVEL 35

#define BAR_PIXELS 89 // Length of x,y bars
#define BAR_X_START 9 // Start point bar x on x-axis
#define BAR_Y_START 34 // Start point bar y on y-axis

#define ANIM_DURATION 100
#define ANIM_DELAY 0

/* **** Global statics **** */
static Window *s_main_window;
static PropertyAnimation *s_box_animation;
static PropertyAnimation *s_box_x_animation;
static PropertyAnimation *s_box_y_animation;
static BitmapLayer *s_bubble_layer;
static BitmapLayer *s_bubble_x_layer;
static BitmapLayer *s_bubble_y_layer;
static BitmapLayer *s_arrangement_layer;
static BitmapLayer *s_mark_x_layer;
static BitmapLayer *s_mark_y_layer;
static GBitmap *s_bubble_bitmap;
static GBitmap *s_bubble_x_bitmap;
static GBitmap *s_bubble_y_bitmap;
static GBitmap *s_arrangement_bitmap;
static GBitmap *s_mark_x_bitmap;
static GBitmap *s_mark_y_bitmap;
static TextLayer *s_output_x_layer;
static TextLayer *s_output_y_layer;
static TextLayer *s_calibrate_layer;

static int16_t	start_x = 0, start_y = 0, start_barx = 0, start_bary = 0;
static int16_t	cal_angle_x = 0, cal_angle_y = 0, cal_loc_x = 0, cal_loc_y = 0; // For caliberation
static int16_t	anglex = 0, angley = 0;
static bool			caliberated = false;

// Square root function (pebble math.h has none)
float my_sqrt( float num )
{
	float a, p, e = 0.001, b;

	a = num;
	p = a * a;
	while( p - num >= e ) {
		b = ( a + ( num / a ) ) / 2;
		a = b;
		p = a * a;
	}
	
	return a;
}

/* **** Animation **** */
static void next_animation(int16_t finish_x, int16_t finish_y) {
  GRect start, finish, rect_start_x, rect_finish_x, rect_start_y, rect_finish_y;

  // Determine start and finish positions of bubble
  start = GRect(start_x, start_y, BOX_SIZE_X, BOX_SIZE_Y);
  finish = GRect(finish_x, finish_y, BOX_SIZE_X, BOX_SIZE_Y);

  // Schedule the next animation of bubble
  s_box_animation = property_animation_create_layer_frame((Layer *)s_bubble_layer, &start, &finish);
  animation_set_duration((Animation*)s_box_animation, ANIM_DURATION);
  animation_set_delay((Animation*)s_box_animation, ANIM_DELAY);
  animation_set_curve((Animation*)s_box_animation, AnimationCurveEaseInOut);
  animation_schedule((Animation*)s_box_animation);

	// Scale since bubble move only for angles in -pi/2 till pi/2
	anglex = (anglex > 90) ? (90) : (anglex);	
	anglex = (anglex < -90) ? (-90) : (anglex);	
	angley = (angley > 90) ? (90) : (angley);	
	angley = (angley < -90) ? (-90) : (angley);	

  // Determine start and finish positions of horizontal dial
  rect_start_x = GRect(start_barx, 5, SMALL_BUBBLE_X, SMALL_BUBBLE_Y);
  rect_finish_x = GRect(BAR_PIXELS/2 + BAR_X_START + anglex/2, 5, SMALL_BUBBLE_X, SMALL_BUBBLE_Y);

  // Schedule the next animation of horizontal dial
  s_box_x_animation = property_animation_create_layer_frame((Layer *)s_bubble_x_layer, &rect_start_x, &rect_finish_x);
  animation_set_duration((Animation*)s_box_x_animation, ANIM_DURATION);
  animation_set_delay((Animation*)s_box_x_animation, ANIM_DELAY);
  animation_set_curve((Animation*)s_box_x_animation, AnimationCurveEaseInOut);
  animation_schedule((Animation*)s_box_x_animation);

  // Determine start and finish positions of vertical dial
  rect_start_y = GRect(8, start_bary, SMALL_BUBBLE_Y, SMALL_BUBBLE_X);
  rect_finish_y = GRect(8, BAR_PIXELS/2 + BAR_Y_START - angley/2, SMALL_BUBBLE_Y, SMALL_BUBBLE_X);

  // Schedule the next animation of vertical dial
  s_box_y_animation = property_animation_create_layer_frame((Layer *)s_bubble_y_layer, &rect_start_y, &rect_finish_y);
  animation_set_duration((Animation*)s_box_y_animation, ANIM_DURATION);
  animation_set_delay((Animation*)s_box_y_animation, ANIM_DELAY);
  animation_set_curve((Animation*)s_box_y_animation, AnimationCurveEaseInOut);
  animation_schedule((Animation*)s_box_y_animation);

	// Save starting points for next interation
	start_x = finish_x;
	start_y = finish_y; 
	start_barx = BAR_PIXELS/2 + BAR_X_START + anglex/2;
	start_bary = BAR_PIXELS/2 + BAR_Y_START - angley/2;
}

/* **** Location is given as coordinates in a square, but bubble is circle **** */
void square_to_circle(int16_t* loc_x, int16_t* loc_y) {	
	double loc_xf, loc_yf, radius;

	loc_xf = (double) *loc_x;
	loc_yf = (double) *loc_y;
	
	radius = my_sqrt(loc_xf*loc_xf + loc_yf*loc_yf);

	if (radius > PRACTICAL_RADIUS) {
		*loc_x = (PRACTICAL_RADIUS* *loc_x )/(int)radius;
		*loc_y = (PRACTICAL_RADIUS* *loc_y )/(int)radius;
	}
}

/* **** Read watch position and analzye x-y positions and angles in each direction **** */
static void data_handler(AccelData *data, uint32_t num_samples) {
	int16_t loc_x, loc_y;
	int16_t value_x = 0, value_y = 0, value_z = 0; 
	int32_t anglex_int32 = 0, angley_int32 = 0;
	int i;

	// Create a long-lived buffer
  static char buffer_angle_x[] = "-000", buffer_angle_y[] = "-000";
	
	// Take an average of the samples
	for ( i = 0; i< (int) num_samples; i = i + 1 ) {	
		value_x = value_x + data[i].x;	
		value_y = value_y + data[i].y;	
		value_z = value_z + data[i].z;	
	}

	value_x = value_x / (int) num_samples;	
	value_y = value_y / (int) num_samples;	
	value_z = value_z / (int) num_samples;	
		
	// Divide milli-g's (range: -1000 to 1000) to approx. 41x41 circle (half radius - width bubble)
	loc_x = -value_x/24;
	loc_y = value_y/24;

	// Use caliberation values (cal_loc_x, cal_loc_y equal zero if not caliberated)
	loc_x = loc_x - cal_loc_x;
	loc_y = loc_y - cal_loc_y;

	// Don't get out of screen
	loc_x = (loc_x > PRACTICAL_RADIUS) ? (PRACTICAL_RADIUS) : (loc_x);	
	loc_y = (loc_y > PRACTICAL_RADIUS) ? (PRACTICAL_RADIUS) : (loc_y);	

	loc_x = (loc_x < -PRACTICAL_RADIUS) ? (-PRACTICAL_RADIUS) : (loc_x);	
	loc_y = (loc_y < -PRACTICAL_RADIUS) ? (-PRACTICAL_RADIUS) : (loc_y);	

	anglex_int32 = atan2_lookup(value_x,value_z);
	anglex = TRIGANGLE_TO_DEG(anglex_int32)-180;
	
	// Angle in (-90,90)
	if (anglex > 90) {
		anglex = anglex - 180;
	} else if (anglex < -90) {
		anglex = anglex + 180;
	}

	anglex = anglex - cal_angle_x; 
	snprintf(buffer_angle_x,sizeof(buffer_angle_x), "%d", anglex);
	text_layer_set_text(s_output_x_layer, buffer_angle_x);

	angley_int32 = atan2_lookup(value_y,value_z);
	angley = TRIGANGLE_TO_DEG(angley_int32)-180;

	// Angle in (-90,90)
	if (angley > 90) {
		angley = angley - 180;
	} else if (angley < -90) {
		angley = angley + 180;
	}

	angley = angley - cal_angle_y; //caliberate
	snprintf(buffer_angle_y,sizeof(buffer_angle_y), "%d", angley);
	text_layer_set_text(s_output_y_layer, buffer_angle_y);

	square_to_circle(&loc_x, &loc_y);
	
// Pixels must be positive + width/length
	loc_x = loc_x + PRACTICAL_RADIUS + MARGIN;
	loc_y = loc_y + PRACTICAL_RADIUS + LINE_LEVEL;
	
	next_animation(loc_x,loc_y);
}

// Turn off light
void turn_off_light(void *data) {
	light_enable(false);
}

// Turn on light for 30 seconds with middle button
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	light_enable(true);
	app_timer_register(30000, turn_off_light, NULL);
}

// Caliberate and uncaliberate
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {

	if (caliberated) {
		cal_loc_x = 0;
		cal_loc_y = 0;
		cal_angle_x = 0;
		cal_angle_y = 0;
	
		text_layer_set_text_color(s_calibrate_layer,GColorBlack);
	  text_layer_set_text(s_calibrate_layer, "Calibrate");

		caliberated = false;
	} else {
		// Save current values
		cal_loc_x = start_x - PRACTICAL_RADIUS - MARGIN;
		cal_loc_y = start_y - PRACTICAL_RADIUS - LINE_LEVEL; 
		cal_angle_x = anglex; 
		cal_angle_y = angley;

		
    #ifdef PBL_COLOR
			text_layer_set_text_color(s_calibrate_layer,GColorRed);
    #endif
	  text_layer_set_text(s_calibrate_layer, "Calibrated");
	
		caliberated = true;
	}
}

// Register the ClickHandlers
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

// Add bitmap to layer, and add layer to window
void add_bitmap_window(Window *window, BitmapLayer *BLayer, GBitmap *BBitmap, uint32_t resource_id, int16_t x, int16_t y, int16_t width, int16_t length) {
	BBitmap = gbitmap_create_with_resource(resource_id);
  BLayer = bitmap_layer_create(GRect( x, y , width, length));
	bitmap_layer_set_bitmap(BLayer, BBitmap);
  bitmap_layer_set_compositing_mode(BLayer, GCompOpSet);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(BLayer));
}

/* **** Main functions **** */
static void main_window_load(Window *window) {
	window_set_click_config_provider(s_main_window, click_config_provider);

	add_bitmap_window(window, s_arrangement_layer, s_arrangement_bitmap, RESOURCE_ID_IMAGE_ARRANGEMENT, 0, 0 , 144, 168);

	// Draw bubble and dials
	s_bubble_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BUBBLE);
  s_bubble_layer = bitmap_layer_create(GRect(0, 0, 12, 12));
  bitmap_layer_set_bitmap(s_bubble_layer, s_bubble_bitmap);
  bitmap_layer_set_compositing_mode(s_bubble_layer,GCompOpSet);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_bubble_layer));

	s_bubble_x_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BUBBLE_X);
  s_bubble_x_layer = bitmap_layer_create(GRect(BAR_PIXELS/2 + BAR_X_START - 5, 6, 8, 20));
	bitmap_layer_set_bitmap(s_bubble_x_layer, s_bubble_x_bitmap);
  bitmap_layer_set_compositing_mode(s_bubble_x_layer,GCompOpSet);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_bubble_x_layer));

	add_bitmap_window(window, s_mark_x_layer, s_mark_x_bitmap, RESOURCE_ID_IMAGE_MARK_X, 48, 6, 18, 18);

	s_bubble_y_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BUBBLE_Y);
  s_bubble_y_layer = bitmap_layer_create(GRect(128, BAR_PIXELS/2 + BAR_Y_START - 5, 20, 8));
	bitmap_layer_set_bitmap(s_bubble_y_layer, s_bubble_y_bitmap);
  bitmap_layer_set_compositing_mode(s_bubble_y_layer,GCompOpSet);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_bubble_y_layer));

	add_bitmap_window(window, s_mark_y_layer, s_mark_y_bitmap, RESOURCE_ID_IMAGE_MARK_Y, 9, 73, 18, 18);

  // Create angles TextLayer
  s_output_x_layer = text_layer_create(GRect(114, 7,  26, 14 ));
  s_output_y_layer = text_layer_create(GRect(4, 140,  26, 14 ));
  text_layer_set_background_color(s_output_x_layer, GColorClear);
  text_layer_set_background_color(s_output_y_layer, GColorClear);
  text_layer_set_text(s_output_x_layer, "25");
  text_layer_set_text(s_output_y_layer, "25");
  text_layer_set_text_alignment(s_output_x_layer, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_output_y_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_output_x_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_output_y_layer));

	// Create Calibrate TextLayer	
  s_calibrate_layer = text_layer_create(GRect(36, 140,  77, 20 ));
  text_layer_set_background_color(s_calibrate_layer, GColorClear);
	text_layer_set_font(s_calibrate_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(s_calibrate_layer, "Calibrate");
  text_layer_set_text_alignment(s_calibrate_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_calibrate_layer));
}

static void main_window_unload(Window *window) {
	// Destroy GBitmap
  gbitmap_destroy(s_bubble_bitmap);
  gbitmap_destroy(s_bubble_x_bitmap);
  gbitmap_destroy(s_bubble_y_bitmap);
  gbitmap_destroy(s_arrangement_bitmap);
  gbitmap_destroy(s_mark_x_bitmap);
  gbitmap_destroy(s_mark_y_bitmap);

  // Destroy BitmapLayer
  bitmap_layer_destroy(s_bubble_layer);
  bitmap_layer_destroy(s_bubble_x_layer);
  bitmap_layer_destroy(s_bubble_y_layer);
  bitmap_layer_destroy(s_arrangement_layer);
  bitmap_layer_destroy(s_mark_x_layer);
  bitmap_layer_destroy(s_mark_y_layer);

  text_layer_destroy(s_output_x_layer);
  text_layer_destroy(s_output_y_layer);
  text_layer_destroy(s_calibrate_layer);

	light_enable(false); //close light when exiting
}

static void init(void) {
  // Create main Window
  s_main_window = window_create();
#ifdef PBL_SDK_2
  window_set_fullscreen(s_main_window, true);
#endif
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
	
	// Subscribe to the accelerometer data service
	int num_samples = 5;
	accel_data_service_subscribe(num_samples, data_handler);
	
	// Choose update rate
  accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
}

static void deinit(void) {
  // Stop any animation in progress
  animation_unschedule_all();

  // Destroy main Window
	window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
