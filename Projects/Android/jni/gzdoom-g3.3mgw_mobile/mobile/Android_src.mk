LOCAL_PATH := $(call my-dir)/../src


include $(CLEAR_VARS)

LOCAL_MODULE    := qzdoom

LOCAL_CFLAGS   :=  -D__MOBILE__ -DNO_PIX_BUFF  -DOPNMIDI_DISABLE_GX_EMULATOR -DGZDOOM  -DLZDOOM -DUSE_GL_HW_BUFFERS  -DNO_VBO -D__STDINT_LIMITS -DENGINE_NAME=\"lzdoom\"


LOCAL_CPPFLAGS := -DHAVE_FLUIDSYNTH -DHAVE_MPG123 -DHAVE_SNDFILE -std=c++14 -DHAVE_JWZGLES  -Wno-inconsistent-missing-override -Werror=format-security  -fexceptions -fpermissive -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -D__forceinline=inline -DNO_GTK -DNO_SSE -fsigned-char

LOCAL_CFLAGS  += -DNO_SEND_STATS

LOCAL_CFLAGS  += -DOPNMIDI_USE_LEGACY_EMULATOR
LOCAL_CFLAGS  += -DADLMIDI_DISABLE_MUS_SUPPORT -DADLMIDI_DISABLE_XMI_SUPPORT -DADLMIDI_DISABLE_MIDI_SEQUENCER
LOCAL_CFLAGS  += -DOPNMIDI_DISABLE_MUS_SUPPORT -DOPNMIDI_DISABLE_XMI_SUPPORT -DOPNMIDI_DISABLE_MIDI_SEQUENCER

ifeq ($(BUILD_SERIAL),1)
LOCAL_CPPFLAGS += -DANTI_HACK 
endif

	
LOCAL_C_INCLUDES := \
 $(TOP_DIR)/ \
 $(GZDOOM_TOP_PATH)/src/  \
 $(GZDOOM_TOP_PATH)/mobile/src/extrafiles  \
 $(GZDOOM_TOP_PATH)/game-music-emu/ \
 $(GZDOOM_TOP_PATH)/gdtoa \
 $(GZDOOM_TOP_PATH)/lzma/C \
 $(GZDOOM_TOP_PATH)/bzip2 \
 $(GZDOOM_TOP_PATH)/asmjit \
 $(GZDOOM_TOP_PATH)/src/sound \
 $(GZDOOM_TOP_PATH)/src/sound/oplsynth \
 $(GZDOOM_TOP_PATH)/src/sound/adlmidi \
 $(GZDOOM_TOP_PATH)/src/sound/opnmidi \
 $(GZDOOM_TOP_PATH)/src/textures \
 $(GZDOOM_TOP_PATH)/src/thingdef \
 $(GZDOOM_TOP_PATH)/src/sdl \
 $(GZDOOM_TOP_PATH)/src/g_inventory \
 $(GZDOOM_TOP_PATH)/src/g_strife \
 $(GZDOOM_TOP_PATH)/src/g_shared \
 $(GZDOOM_TOP_PATH)/src/g_statusbar \
 $(GZDOOM_TOP_PATH)/src/scripting \
 $(GZDOOM_TOP_PATH)/src/scripting/vm \
 $(GZDOOM_TOP_PATH)/src/posix \
 $(GZDOOM_TOP_PATH)/src/posix\sdl \
 $(SDL_INCLUDE_PATHS) \
 $(SUPPORT_LIBS)/fluidsynth-lite/include \
 $(SUPPORT_LIBS)/openal/include/AL \
 $(SUPPORT_LIBS)/libsndfile-android/jni/ \
 $(SUPPORT_LIBS)/libmpg123 \
 $(SUPPORT_LIBS)/jpeg8d \
 $(SUPPORT_LIBS)/gl4es/include \
 $(GZDOOM_TOP_PATH)/mobile/src \
 $(GL4ES_PATH)


#############################################################################
# CLIENT/SERVER
#############################################################################


ANDROID_SRC_FILES = \
    ../mobile/src/i_specialpaths_android.cpp
#    ../../../Clibs_OpenTouch/idtech1/gzdoom_game_interface.cpp \
#    ../../../Clibs_OpenTouch/idtech1/touch_interface.cpp \
#    ../../../Clibs_OpenTouch/idtech1/android_jni.cpp \

PLAT_POSIX_SOURCES = \
	posix/i_cd.cpp \
	posix/i_steam.cpp

PLAT_SDL_SOURCES = \
	posix/sdl/crashcatcher.c \
	posix/sdl/hardware.cpp \
	posix/sdl/i_gui.cpp \
	posix/sdl/i_input.cpp \
	posix/sdl/i_joystick.cpp \
	posix/sdl/i_main.cpp \
	posix/sdl/i_system.cpp \
	posix/sdl/sdlglvideo.cpp \
	posix/sdl/sdlvideo.cpp \
	posix/sdl/st_start.cpp

SWRENDER_SOURCES = \
	swrenderer/r_swcanvas.cpp \
	swrenderer/r_swcolormaps.cpp \
	swrenderer/r_swrenderer.cpp \
	swrenderer/r_memory.cpp \
	swrenderer/r_renderthread.cpp \
	swrenderer/drawers/r_draw.cpp \
	swrenderer/drawers/r_draw_pal.cpp \
	swrenderer/drawers/r_draw_rgba.cpp \
	swrenderer/drawers/r_thread.cpp \
	swrenderer/scene/r_3dfloors.cpp \
	swrenderer/scene/r_light.cpp \
	swrenderer/scene/r_opaque_pass.cpp \
	swrenderer/scene/r_portal.cpp \
	swrenderer/scene/r_scene.cpp \
	swrenderer/scene/r_translucent_pass.cpp \
	swrenderer/viewport/r_drawerargs.cpp \
	swrenderer/viewport/r_skydrawer.cpp \
	swrenderer/viewport/r_spandrawer.cpp \
	swrenderer/viewport/r_spritedrawer.cpp \
	swrenderer/viewport/r_viewport.cpp \
	swrenderer/viewport/r_walldrawer.cpp \
	swrenderer/line/r_line.cpp \
	swrenderer/line/r_farclip_line.cpp \
	swrenderer/line/r_walldraw.cpp \
	swrenderer/line/r_wallsetup.cpp \
	swrenderer/line/r_fogboundary.cpp \
	swrenderer/line/r_renderdrawsegment.cpp \
	swrenderer/segments/r_clipsegment.cpp \
	swrenderer/segments/r_drawsegment.cpp \
	swrenderer/segments/r_portalsegment.cpp \
	swrenderer/things/r_visiblesprite.cpp \
	swrenderer/things/r_visiblespritelist.cpp \
	swrenderer/things/r_voxel.cpp \
	swrenderer/things/r_particle.cpp \
	swrenderer/things/r_playersprite.cpp \
	swrenderer/things/r_sprite.cpp \
	swrenderer/things/r_wallsprite.cpp \
	swrenderer/things/r_decal.cpp \
	swrenderer/things/r_model.cpp \
	swrenderer/plane/r_visibleplane.cpp \
	swrenderer/plane/r_visibleplanelist.cpp \
	swrenderer/plane/r_skyplane.cpp \
	swrenderer/plane/r_planerenderer.cpp \
	swrenderer/plane/r_flatplane.cpp \
	swrenderer/plane/r_slopeplane.cpp \

FASTMATH_SOURCES = \
	swrenderer/r_all.cpp \
	polyrenderer/poly_all.cpp \
	sound/oplsynth/opl_mus_player.cpp \
	sound/mpg123_decoder.cpp \
	sound/music_midi_base.cpp \
	sound/oalsound.cpp \
	sound/sndfile_decoder.cpp \
	sound/timiditypp/fft4g.cpp \
	sound/timiditypp/reverb.cpp \
	gl/utility/gl_clock.cpp \
	gl/renderer/gl_2ddrawer.cpp \
	gl/hqnx/init.cpp \
	gl/hqnx/hq2x.cpp \
	gl/hqnx/hq3x.cpp \
	gl/hqnx/hq4x.cpp \
	gl/xbr/xbrz.cpp \
	gl/xbr/xbrz_old.cpp \
	gl/scene/gl_bsp.cpp \
	gl/scene/gl_fakeflat.cpp \
	gl/scene/gl_clipper.cpp \
	gl/scene/gl_decal.cpp \
	gl/scene/gl_drawinfo.cpp \
	gl/scene/gl_flats.cpp \
	gl/scene/gl_walls.cpp \
	gl/scene/gl_sprite.cpp \
	gl/scene/gl_skydome.cpp \
	gl/scene/gl_renderhacks.cpp \
	gl/scene/gl_weapon.cpp \
	gl/scene/gl_scene.cpp \
	gl/scene/gl_sky.cpp \
	gl/scene/gl_portal.cpp \
	gl/scene/gl_walls_draw.cpp \
	gl/scene/gl_vertex.cpp \
	gl/scene/gl_spritelight.cpp \
	gl/dynlights/gl_dynlight1.cpp \
	gl/system/gl_load.c \
	gl/models/gl_models.cpp \
	r_data/models/models.cpp \
	r_data/matrix.cpp \
	sound/adlmidi/adldata.cpp \
	sound/adlmidi/adlmidi.cpp \
	sound/adlmidi/adlmidi_load.cpp \
	sound/adlmidi/adlmidi_midiplay.cpp \
	sound/adlmidi/adlmidi_opl3.cpp \
	sound/adlmidi/adlmidi_private.cpp \
	sound/adlmidi/chips/dosbox/dbopl.cpp \
	sound/adlmidi/chips/dosbox_opl3.cpp \
	sound/adlmidi/chips/nuked/nukedopl3_174.c \
	sound/adlmidi/chips/nuked/nukedopl3.c \
	sound/adlmidi/chips/nuked_opl3.cpp \
	sound/adlmidi/chips/nuked_opl3_v174.cpp \
	sound/adlmidi/wopl/wopl_file.c \
	sound/opnmidi/chips/gens_opn2.cpp \
	sound/opnmidi/chips/gens/Ym2612_Emu.cpp \
	sound/opnmidi/chips/mame/mame_ym2612fm.c \
	sound/opnmidi/chips/mame_opn2.cpp \
	sound/opnmidi/chips/nuked_opn2.cpp \
	sound/opnmidi/chips/nuked/ym3438.c \
	sound/opnmidi/opnmidi.cpp \
	sound/opnmidi/opnmidi_load.cpp \
	sound/opnmidi/opnmidi_midiplay.cpp \
	sound/opnmidi/opnmidi_opn2.cpp \
	sound/opnmidi/opnmidi_private.cpp \
	sound/opnmidi/wopn/wopn_file.c



PCH_SOURCES = \
	actorptrselect.cpp \
	am_map.cpp \
	b_bot.cpp \
	b_func.cpp \
	b_game.cpp \
	b_move.cpp \
	b_think.cpp \
	bbannouncer.cpp \
	c_bind.cpp \
	c_cmds.cpp \
	c_console.cpp \
	c_consolebuffer.cpp \
	c_cvars.cpp \
	c_dispatch.cpp \
	c_expr.cpp \
	c_functions.cpp \
	cmdlib.cpp \
	colormatcher.cpp \
	compatibility.cpp \
	configfile.cpp \
	ct_chat.cpp \
	cycler.cpp \
	d_dehacked.cpp \
	d_iwad.cpp \
	d_main.cpp \
	d_stats.cpp \
	d_net.cpp \
	d_netinfo.cpp \
	d_protocol.cpp \
	decallib.cpp \
	dobject.cpp \
	dobjgc.cpp \
	dobjtype.cpp \
	doomstat.cpp \
	dsectoreffect.cpp \
	dthinker.cpp \
	edata.cpp \
	f_wipe.cpp \
	files.cpp \
	files_decompress.cpp \
	g_doomedmap.cpp \
	g_game.cpp \
	g_hub.cpp \
	g_level.cpp \
	g_mapinfo.cpp \
	g_skill.cpp \
	gameconfigfile.cpp \
	gi.cpp \
	gitinfo.cpp \
	hu_scores.cpp \
	i_module.cpp \
	i_net.cpp \
	i_time.cpp \
	info.cpp \
	keysections.cpp \
	lumpconfigfile.cpp \
	m_alloc.cpp \
	m_argv.cpp \
	m_bbox.cpp \
	m_cheat.cpp \
	m_joy.cpp \
	m_misc.cpp \
	m_png.cpp \
	m_random.cpp \
	memarena.cpp \
	md5.cpp \
	name.cpp \
	nodebuild.cpp \
	nodebuild_classify_nosse2.cpp \
	nodebuild_events.cpp \
	nodebuild_extract.cpp \
	nodebuild_gl.cpp \
	nodebuild_utility.cpp \
	p_3dfloors.cpp \
	p_3dmidtex.cpp \
	p_acs.cpp \
	p_actionfunctions.cpp \
	p_ceiling.cpp \
	p_conversation.cpp \
	p_destructible.cpp \
	p_doors.cpp \
	p_effect.cpp \
	p_enemy.cpp \
	p_floor.cpp \
	p_glnodes.cpp \
	p_interaction.cpp \
	p_lights.cpp \
	p_linkedsectors.cpp \
	p_lnspec.cpp \
	p_map.cpp \
	p_maputl.cpp \
	p_mobj.cpp \
	p_openmap.cpp \
	p_pillar.cpp \
	p_plats.cpp \
	p_pspr.cpp \
	p_pusher.cpp \
	p_saveg.cpp \
	p_scroll.cpp \
	p_secnodes.cpp \
	p_sectors.cpp \
	p_setup.cpp \
	p_sight.cpp \
	p_slopes.cpp \
	p_spec.cpp \
	p_states.cpp \
	p_switch.cpp \
	p_tags.cpp \
	p_teleport.cpp \
	p_terrain.cpp \
	p_things.cpp \
	p_tick.cpp \
	p_trace.cpp \
	p_udmf.cpp \
	p_usdf.cpp \
	p_user.cpp \
	p_xlat.cpp \
	parsecontext.cpp \
	po_man.cpp \
	portal.cpp \
	r_utility.cpp \
	r_sky.cpp \
	r_videoscale.cpp \
	s_advsound.cpp \
	s_environment.cpp \
	s_playlist.cpp \
	s_sndseq.cpp \
	s_sound.cpp \
	serializer.cpp \
	sc_man.cpp \
	scriptutil.cpp \
	st_stuff.cpp \
	statistics.cpp \
	stats.cpp \
	stringtable.cpp \
	teaminfo.cpp \
	umapinfo.cpp \
	v_blend.cpp \
	v_collection.cpp \
	v_draw.cpp \
	v_font.cpp \
	v_palette.cpp \
	v_pfx.cpp \
	v_text.cpp \
	v_video.cpp \
	w_wad.cpp \
	wi_stuff.cpp \
	utf8.cpp \
	zstrformat.cpp \
	g_inventory/a_keys.cpp \
	g_inventory/a_pickups.cpp \
	g_inventory/a_weapons.cpp \
	g_shared/a_action.cpp \
	g_shared/a_decals.cpp \
	g_shared/a_dynlight.cpp \
	g_shared/a_dynlightdata.cpp \
	g_shared/a_flashfader.cpp \
	g_shared/a_lightning.cpp \
	g_shared/a_morph.cpp \
	g_shared/a_quake.cpp \
	g_shared/a_specialspot.cpp \
	g_shared/hudmessages.cpp \
	g_shared/shared_hud.cpp \
	g_statusbar/sbarinfo.cpp \
	g_statusbar/sbar_mugshot.cpp \
	g_statusbar/shared_sbar.cpp \
	gl/compatibility/gl_20.cpp \
	gl/data/gl_data.cpp \
	gl/data/gl_portaldata.cpp \
	gl/data/gl_setup.cpp \
	gl/data/gl_vertexbuffer.cpp \
	gl/dynlights/gl_glow.cpp \
	gl/dynlights/gl_lightbuffer.cpp \
	gl/dynlights/gl_aabbtree.cpp \
	gl/dynlights/gl_shadowmap.cpp \
	gl/renderer/gl_quaddrawer.cpp \
	gl/renderer/gl_renderer.cpp \
	gl/renderer/gl_renderstate.cpp \
	gl/renderer/gl_renderbuffers.cpp \
	gl/renderer/gl_lightdata.cpp \
	gl/renderer/gl_postprocess.cpp \
	gl/renderer/gl_postprocessstate.cpp \
	gl/shaders/gl_shader.cpp \
	gl/shaders/gl_texshader.cpp \
	gl/shaders/gl_shaderprogram.cpp \
	gl/shaders/gl_postprocessshader.cpp \
	gl/shaders/gl_shadowmapshader.cpp \
	gl/shaders/gl_presentshader.cpp \
	gl/shaders/gl_present3dRowshader.cpp \
	gl/shaders/gl_bloomshader.cpp \
	gl/shaders/gl_ambientshader.cpp \
	gl/shaders/gl_blurshader.cpp \
	gl/shaders/gl_colormapshader.cpp \
	gl/shaders/gl_tonemapshader.cpp \
	gl/shaders/gl_lensshader.cpp \
	gl/shaders/gl_fxaashader.cpp \
	gl/stereo3d/gl_stereo3d.cpp \
	gl/stereo3d/gl_stereo_cvars.cpp \
	gl/stereo3d/gl_stereo_leftright.cpp \
	gl/stereo3d/scoped_view_shifter.cpp \
	gl/stereo3d/gl_anaglyph.cpp \
	gl/stereo3d/gl_quadstereo.cpp \
	gl/stereo3d/gl_sidebyside3d.cpp \
	gl/stereo3d/gl_interleaved3d.cpp \
	gl/system/gl_interface.cpp \
	gl/system/gl_framebuffer.cpp \
	gl/system/gl_swframebuffer.cpp \
	gl/system/gl_swwipe.cpp \
	gl/system/gl_debug.cpp \
	gl/system/gl_menu.cpp \
	gl/system/gl_wipe.cpp \
	gl/textures/gl_hwtexture.cpp \
	gl/textures/gl_texture.cpp \
	gl/textures/gl_material.cpp \
	gl/textures/gl_hirestex.cpp \
	gl/textures/gl_samplers.cpp \
	gl/textures/gl_translate.cpp \
	gl/textures/gl_hqresize.cpp \
	menu/joystickmenu.cpp \
	menu/loadsavemenu.cpp \
	menu/menu.cpp \
	menu/menudef.cpp \
	menu/messagebox.cpp \
	menu/optionmenu.cpp \
	menu/playermenu.cpp \
	menu/videomenu.cpp \
	resourcefiles/ancientzip.cpp \
	resourcefiles/file_7z.cpp \
	resourcefiles/file_grp.cpp \
	resourcefiles/file_lump.cpp \
	resourcefiles/file_rff.cpp \
	resourcefiles/file_wad.cpp \
	resourcefiles/file_zip.cpp \
	resourcefiles/file_pak.cpp \
	resourcefiles/file_directory.cpp \
	resourcefiles/resourcefile.cpp \
	textures/animations.cpp \
	textures/anim_switches.cpp \
	textures/automaptexture.cpp \
	textures/bitmap.cpp \
	textures/buildtexture.cpp \
	textures/canvastexture.cpp \
	textures/ddstexture.cpp \
	textures/flattexture.cpp \
	textures/imgztexture.cpp \
	textures/jpegtexture.cpp \
	textures/md5check.cpp \
	textures/multipatchtexture.cpp \
	textures/patchtexture.cpp \
	textures/pcxtexture.cpp \
	textures/pngtexture.cpp \
	textures/rawpagetexture.cpp \
	textures/emptytexture.cpp \
	textures/shadertexture.cpp \
	textures/texture.cpp \
	textures/texturemanager.cpp \
	textures/tgatexture.cpp \
	textures/warptexture.cpp \
	textures/skyboxtexture.cpp \
	textures/worldtexture.cpp \
	xlat/parse_xlat.cpp \
	fragglescript/t_func.cpp \
	fragglescript/t_load.cpp \
	fragglescript/t_oper.cpp \
	fragglescript/t_parse.cpp \
	fragglescript/t_prepro.cpp \
	fragglescript/t_script.cpp \
	fragglescript/t_spec.cpp \
	fragglescript/t_variable.cpp \
	fragglescript/t_cmd.cpp \
	intermission/intermission.cpp \
	intermission/intermission_parse.cpp \
	r_data/colormaps.cpp \
	r_data/gldefs.cpp \
	r_data/r_translate.cpp \
	r_data/sprites.cpp \
	r_data/voxels.cpp \
	r_data/renderstyle.cpp \
	r_data/r_interpolate.cpp \
	r_data/r_vanillatrans.cpp \
	r_data/models/models_md3.cpp \
	r_data/models/models_md2.cpp \
	r_data/models/models_voxel.cpp \
	r_data/models/models_ue1.cpp \
	r_data/models/models_obj.cpp \
	scripting/symbols.cpp \
	scripting/vmiterators.cpp \
	scripting/vmthunks.cpp \
	scripting/vmthunks_actors.cpp \
	scripting/types.cpp \
	scripting/thingdef.cpp \
	scripting/thingdef_data.cpp \
	scripting/thingdef_properties.cpp \
	scripting/backend/codegen.cpp \
	scripting/backend/scopebarrier.cpp \
	scripting/backend/dynarrays.cpp \
	scripting/backend/vmbuilder.cpp \
	scripting/backend/vmdisasm.cpp \
	scripting/decorate/olddecorations.cpp \
	scripting/decorate/thingdef_exp.cpp \
	scripting/decorate/thingdef_parse.cpp \
	scripting/decorate/thingdef_states.cpp \
	scripting/vm/vmexec.cpp \
	scripting/vm/vmframe.cpp \
	scripting/zscript/ast.cpp \
	scripting/zscript/zcc_compile.cpp \
	scripting/zscript/zcc_parser.cpp \
	sfmt/SFMT.cpp \
	sound/i_music.cpp \
	sound/i_sound.cpp \
	sound/i_soundfont.cpp \
	sound/s_music.cpp \
	sound/mididevices/music_adlmidi_mididevice.cpp \
	sound/mididevices/music_opldumper_mididevice.cpp \
	sound/mididevices/music_opl_mididevice.cpp \
	sound/mididevices/music_opnmidi_mididevice.cpp \
	sound/mididevices/music_timiditypp_mididevice.cpp \
	sound/mididevices/music_fluidsynth_mididevice.cpp \
	sound/mididevices/music_softsynth_mididevice.cpp \
	sound/mididevices/music_timidity_mididevice.cpp \
	sound/mididevices/music_wildmidi_mididevice.cpp \
	sound/mididevices/music_wavewriter_mididevice.cpp \
	sound/midisources/midisource.cpp \
	sound/midisources/midisource_mus.cpp \
	sound/midisources/midisource_smf.cpp \
	sound/midisources/midisource_hmi.cpp \
	sound/midisources/midisource_xmi.cpp \
	sound/musicformats/music_xa.cpp \
	sound/musicformats/music_cd.cpp \
	sound/musicformats/music_dumb.cpp \
	sound/musicformats/music_gme.cpp \
	sound/musicformats/music_libsndfile.cpp \
	sound/musicformats/music_midistream.cpp \
	sound/musicformats/music_opl.cpp \
	sound/musicformats/music_stream.cpp \
	sound/oplsynth/fmopl.cpp \
	sound/oplsynth/musicblock.cpp \
	sound/oplsynth/oplio.cpp \
	sound/oplsynth/dosbox/opl.cpp \
	sound/oplsynth/OPL3.cpp \
	sound/oplsynth/nukedopl3.cpp \
	sound/timidity/common.cpp \
	sound/timidity/instrum.cpp \
	sound/timidity/instrum_dls.cpp \
	sound/timidity/instrum_font.cpp \
	sound/timidity/instrum_sf2.cpp \
	sound/timidity/mix.cpp \
	sound/timidity/playmidi.cpp \
	sound/timidity/resample.cpp \
	sound/timidity/timidity.cpp \
	sound/timiditypp/common.cpp \
	sound/timiditypp/configfile.cpp \
	sound/timiditypp/effect.cpp \
	sound/timiditypp/filter.cpp \
	sound/timiditypp/freq.cpp \
	sound/timiditypp/instrum.cpp \
	sound/timiditypp/mblock.cpp \
	sound/timiditypp/mix.cpp \
	sound/timiditypp/playmidi.cpp \
	sound/timiditypp/quantity.cpp \
	sound/timiditypp/readmidic.cpp \
	sound/timiditypp/recache.cpp \
	sound/timiditypp/resample.cpp \
	sound/timiditypp/sbkconv.cpp \
	sound/timiditypp/sffile.cpp \
	sound/timiditypp/sfitem.cpp \
	sound/timiditypp/smplfile.cpp \
	sound/timiditypp/sndfont.cpp \
	sound/timiditypp/tables.cpp \
	sound/wildmidi/file_io.cpp \
	sound/wildmidi/gus_pat.cpp \
	sound/wildmidi/reverb.cpp \
	sound/wildmidi/wildmidi_lib.cpp \
	sound/wildmidi/wm_error.cpp \
	events.cpp \
	atterm.cpp \
	GuillotineBinPack.cpp \
	SkylineBinPack.cpp \
	

QZDOOM_SRC = \
   ../../QzDoom/QzDoom_SurfaceView.c \
   ../../QzDoom/VrCompositor.c \
   ../../QzDoom/VrInputCommon.c \
   ../../QzDoom/VrInputDefault.c \
   ../../QzDoom/mathlib.c \
   ../../QzDoom/matrixlib.c \
   ../../QzDoom/argtable3.c


LOCAL_SRC_FILES = \
    __autostart.cpp \
    $(QZDOOM_SRC) \
    $(ANDROID_SRC_FILES) \
    $(PLAT_POSIX_SOURCES) \
    $(PLAT_SDL_SOURCES) \
    $(FASTMATH_SOURCES) \
    $(PCH_SOURCES) \
	x86.cpp \
	strnatcmp.c \
	zstring.cpp \
	math/asin.c \
	math/atan.c \
	math/const.c \
	math/cosh.c \
	math/exp.c \
	math/isnan.c \
	math/log.c \
	math/log10.c \
	math/mtherr.c \
	math/polevl.c \
	math/pow.c \
	math/powi.c \
	math/sin.c \
	math/sinh.c \
	math/sqrt.c \
	math/tan.c \
	math/tanh.c \
	math/fastsin.cpp \
	zzautozend.cpp


# Turn down optimisation of this file so clang doesnt produce ldrd instructions which are missaligned
p_acs.cpp_CFLAGS := -O1

LOCAL_LDLIBS := -ldl -llog -lOpenSLES -landroid
#LOCAL_LDLIBS +=-lGLESv1_CM
LOCAL_LDLIBS += -lGLESv3

LOCAL_LDLIBS +=  -lEGL

# This is stop a linker warning for mp123 lib failing build
#LOCAL_LDLIBS += -Wl,--no-warn-shared-textrel

LOCAL_STATIC_LIBRARIES :=  sndfile mpg123 fluidsynth-static SDL2_net libjpeg zlib_lz lzma_lz gdtoa_lz dumb_lz gme_lz bzip2_lz 
LOCAL_SHARED_LIBRARIES :=  openal SDL2 vrapi

LOCAL_STATIC_LIBRARIES +=

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)


