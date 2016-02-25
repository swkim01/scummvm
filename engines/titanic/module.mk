MODULE := engines/titanic

MODULE_OBJS := \
	compressed_file.o \
	compression.o \
	detection.o \
	direct_draw.o \
	font.o \
	game_manager.o \
	game_view.o \
	image.o \
	main_game_window.o \
	screen_manager.o \
	simple_file.o \
	string.o \
	titanic.o \
	video_surface.o \
	objects/auto_sound_event.o \
	objects/dont_save_file_item.o \
	objects/file_item.o \
	objects/game_object.o \
	objects/link_item.o \
	objects/list.o \
	objects/message_target.o \
	objects/movie_clip.o \
	objects/named_item.o \
	objects/node_item.o \
	objects/pet_control.o \
	objects/project_item.o \
	objects/resource_key.o \
	objects/saveable_object.o \
	objects/tree_item.o \
	objects/view_item.o \
	rooms/announce.o \
	rooms/door_auto_sound_event.o \
	rooms/pet_position.o \
	rooms/room_item.o \
	rooms/service_elevator_door.o \
	rooms/sub_glass.o

# This module can be built as a plugin
ifeq ($(ENABLE_TITANIC), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk
