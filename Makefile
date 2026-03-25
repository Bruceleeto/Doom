# dhewm3 Makefile - 32-bit Linux, hardlinked game code
#
#   make              - RelWithDebInfo build
#   make DEBUG=1      - debug build
#   make NOCURL=1     - build without libcurl
#   make clean

CC  := gcc
CXX := g++

BINARY   := doom
BUILDDIR := build
SRCDIR   := neo

# ---- 32-bit ----
ARCH := -m32

# ---- Paths (change these if your libs live elsewhere) ----
SDL2_INCDIR   := /usr/local/include/SDL2
SDL2_LIBDIR   := /usr/local/lib
OPENAL_INCDIR := /usr/include/AL
OPENAL_LIBDIR := /usr/lib
CURL_LIBDIR   := /usr/lib

# ---- Feature toggles ----
NOCURL ?= 0
# ---- Defines ----
DEFINES := -DD3_ARCH=\"x86\" -DD3_SIZEOFPTR=4 -DD3_OSTYPE=\"linux\" -DD3_IS_BIG_ENDIAN=0 \
           -DIMGUI_DISABLE \
           -DBUILD_OS=\"linux\" -DBUILD_CPU=\"x86\" \
           -DBUILD_LIBRARY_SUFFIX=\".so\" \
           -DBUILD_LIBDIR=\"/usr/local/lib/doom\" \
           -DBUILD_DATADIR=\"/usr/local/share/doom\" \
           -DID_ENABLE_CURL

ifneq ($(NOCURL),0)
  DEFINES += -UID_ENABLE_CURL
endif

GAME_INCDIR := $(SRCDIR)/game

# ---- Includes ----
INCLUDES := -I$(SRCDIR) -I$(SDL2_INCDIR) -I$(OPENAL_INCDIR) -I$(GAME_INCDIR)

# ---- Compiler flags (matched to working CMake build) ----
COMMON := $(ARCH) -pipe -Wall -march=pentium3 \
          -fno-strict-aliasing -ffp-contract=off \
          -fvisibility=hidden -Wno-sign-compare -Wno-switch

ifdef DEBUG
  OPT := -g -ggdb -D_DEBUG -O0
else
  OPT := -g -ggdb -O2 -fno-math-errno -fno-trapping-math -ffinite-math-only -fno-omit-frame-pointer
endif

CFLAGS   := $(COMMON) $(OPT) $(INCLUDES) $(DEFINES)
CXXFLAGS := $(COMMON) $(OPT) $(INCLUDES) $(DEFINES) -std=gnu++11 \
            -Werror=dangling-reference -Woverloaded-virtual \
            -Wno-class-memaccess -Wno-c++20-compat

LDFLAGS := $(ARCH)
LIBS    := -L$(SDL2_LIBDIR) -lSDL2 -L$(OPENAL_LIBDIR) -lopenal -lpthread
ifeq ($(NOCURL),0)
  LIBS += -L$(CURL_LIBDIR) -lcurl
endif

# ============================================================================
# Sources (relative to SRCDIR)
# ============================================================================

SRC_RENDERER := \
	renderer/Cinematic.cpp \
	renderer/GuiModel.cpp \
	renderer/Image_files.cpp \
	renderer/Image_init.cpp \
	renderer/Image_load.cpp \
	renderer/Image_process.cpp \
	renderer/Image_program.cpp \
	renderer/Interaction.cpp \
	renderer/Material.cpp \
	renderer/Model.cpp \
	renderer/ModelDecal.cpp \
	renderer/ModelManager.cpp \
	renderer/ModelOverlay.cpp \
	renderer/Model_beam.cpp \
	renderer/Model_ase.cpp \
	renderer/Model_liquid.cpp \
	renderer/Model_lwo.cpp \
	renderer/Model_ma.cpp \
	renderer/Model_md3.cpp \
	renderer/Model_md5.cpp \
	renderer/Model_prt.cpp \
	renderer/Model_sprite.cpp \
	renderer/RenderEntity.cpp \
	renderer/RenderSystem.cpp \
	renderer/RenderSystem_init.cpp \
	renderer/RenderWorld.cpp \
	renderer/RenderWorld_demo.cpp \
	renderer/RenderWorld_load.cpp \
	renderer/RenderWorld_portals.cpp \
	renderer/VertexCache.cpp \
	renderer/draw_arb2.cpp \
	renderer/draw_common.cpp \
	renderer/tr_backend.cpp \
	renderer/tr_deform.cpp \
	renderer/tr_font.cpp \
	renderer/tr_guisurf.cpp \
	renderer/tr_light.cpp \
	renderer/tr_lightrun.cpp \
	renderer/tr_main.cpp \
	renderer/tr_orderIndexes.cpp \
	renderer/tr_polytope.cpp \
	renderer/tr_render.cpp \
	renderer/tr_rendertools.cpp \
	renderer/tr_shadowbounds.cpp \
	renderer/tr_stencilshadow.cpp \
	renderer/tr_subview.cpp \
	renderer/tr_trace.cpp \
	renderer/tr_trisurf.cpp \
	renderer/tr_turboshadow.cpp

SRC_RENDERER_C := renderer/stblib_impls.c

SRC_FRAMEWORK := \
	framework/CVarSystem.cpp \
	framework/CmdSystem.cpp \
	framework/Common.cpp \
	framework/Compressor.cpp \
	framework/Console.cpp \
	framework/DemoFile.cpp \
	framework/DeclAF.cpp \
	framework/DeclEntityDef.cpp \
	framework/DeclFX.cpp \
	framework/DeclManager.cpp \
	framework/DeclParticle.cpp \
	framework/DeclPDA.cpp \
	framework/DeclSkin.cpp \
	framework/DeclTable.cpp \
	framework/EditField.cpp \
	framework/EventLoop.cpp \
	framework/File.cpp \
	framework/FileSystem.cpp \
	framework/KeyInput.cpp \
	framework/UsercmdGen.cpp \
	framework/Session_menu.cpp \
	framework/Session.cpp \
	framework/async/AsyncClient.cpp \
	framework/async/AsyncNetwork.cpp \
	framework/async/AsyncServer.cpp \
	framework/async/MsgChannel.cpp \
	framework/async/NetworkSystem.cpp \
	framework/async/ServerScan.cpp \
	framework/minizip/unzip.cpp

SRC_FRAMEWORK_C := \
	framework/miniz/miniz.c \
	framework/minizip/ioapi.c

SRC_CM := \
	cm/CollisionModel_contacts.cpp \
	cm/CollisionModel_contents.cpp \
	cm/CollisionModel_debug.cpp \
	cm/CollisionModel_files.cpp \
	cm/CollisionModel_load.cpp \
	cm/CollisionModel_rotate.cpp \
	cm/CollisionModel_trace.cpp \
	cm/CollisionModel_translate.cpp

SRC_AAS := \
	tools/compilers/aas/AASFile.cpp \
	tools/compilers/aas/AASFile_optimize.cpp \
	tools/compilers/aas/AASFile_sample.cpp \
	tools/compilers/aas/AASFileManager.cpp

SRC_SOUND := \
	sound/snd_cache.cpp \
	sound/snd_decoder.cpp \
	sound/snd_efxfile.cpp \
	sound/snd_emitter.cpp \
	sound/snd_shader.cpp \
	sound/snd_system.cpp \
	sound/snd_wavefile.cpp \
	sound/snd_world.cpp

SRC_SOUND_C := sound/stbvorbis_impl.c

SRC_UI := \
	ui/BindWindow.cpp \
	ui/ChoiceWindow.cpp \
	ui/DeviceContext.cpp \
	ui/EditWindow.cpp \
	ui/FieldWindow.cpp \
	ui/GameBearShootWindow.cpp \
	ui/GameBustOutWindow.cpp \
	ui/GameSSDWindow.cpp \
	ui/GuiScript.cpp \
	ui/ListGUI.cpp \
	ui/ListWindow.cpp \
	ui/MarkerWindow.cpp \
	ui/RegExp.cpp \
	ui/RenderWindow.cpp \
	ui/SimpleWindow.cpp \
	ui/SliderWindow.cpp \
	ui/UserInterface.cpp \
	ui/Window.cpp \
	ui/Winvar.cpp

SRC_IDLIB := \
	idlib/bv/Bounds.cpp \
	idlib/bv/Frustum.cpp \
	idlib/bv/Sphere.cpp \
	idlib/bv/Box.cpp \
	idlib/geometry/DrawVert.cpp \
	idlib/geometry/Winding2D.cpp \
	idlib/geometry/Surface_SweptSpline.cpp \
	idlib/geometry/Winding.cpp \
	idlib/geometry/Surface.cpp \
	idlib/geometry/Surface_Patch.cpp \
	idlib/geometry/TraceModel.cpp \
	idlib/geometry/JointTransform.cpp \
	idlib/hashing/CRC32.cpp \
	idlib/hashing/MD4.cpp \
	idlib/hashing/MD5.cpp \
	idlib/math/Angles.cpp \
	idlib/math/Lcp.cpp \
	idlib/math/Math.cpp \
	idlib/math/Matrix.cpp \
	idlib/math/Ode.cpp \
	idlib/math/Plane.cpp \
	idlib/math/Pluecker.cpp \
	idlib/math/Polynomial.cpp \
	idlib/math/Quat.cpp \
	idlib/math/Rotation.cpp \
	idlib/math/Simd.cpp \
	idlib/math/Simd_Generic.cpp \
	idlib/math/Simd_AltiVec.cpp \
	idlib/math/Simd_MMX.cpp \
	idlib/math/Simd_3DNow.cpp \
	idlib/math/Simd_SSE.cpp \
	idlib/math/Simd_SSE2.cpp \
	idlib/math/Simd_SSE3.cpp \
	idlib/math/Vector.cpp \
	idlib/BitMsg.cpp \
	idlib/LangDict.cpp \
	idlib/Lexer.cpp \
	idlib/Lib.cpp \
	idlib/containers/HashIndex.cpp \
	idlib/Dict.cpp \
	idlib/Str.cpp \
	idlib/Parser.cpp \
	idlib/MapFile.cpp \
	idlib/CmdArgs.cpp \
	idlib/Token.cpp \
	idlib/Base64.cpp \
	idlib/Timer.cpp \
	idlib/Heap.cpp

SRC_GAME := \
	game/AF.cpp \
	game/AFEntity.cpp \
	game/Actor.cpp \
	game/Camera.cpp \
	game/Entity.cpp \
	game/BrittleFracture.cpp \
	game/Fx.cpp \
	game/GameEdit.cpp \
	game/Game_local.cpp \
	game/Game_network.cpp \
	game/Item.cpp \
	game/IK.cpp \
	game/Light.cpp \
	game/Misc.cpp \
	game/Mover.cpp \
	game/Moveable.cpp \
	game/MultiplayerGame.cpp \
	game/Player.cpp \
	game/PlayerIcon.cpp \
	game/PlayerView.cpp \
	game/Projectile.cpp \
	game/Pvs.cpp \
	game/SecurityCamera.cpp \
	game/SmokeParticles.cpp \
	game/Sound.cpp \
	game/Target.cpp \
	game/Trigger.cpp \
	game/Weapon.cpp \
	game/WorldSpawn.cpp \
	game/ai/AAS.cpp \
	game/ai/AAS_debug.cpp \
	game/ai/AAS_pathing.cpp \
	game/ai/AAS_routing.cpp \
	game/ai/AI.cpp \
	game/ai/AI_events.cpp \
	game/ai/AI_pathing.cpp \
	game/ai/AI_Vagary.cpp \
	game/gamesys/DebugGraph.cpp \
	game/gamesys/Class.cpp \
	game/gamesys/Event.cpp \
	game/gamesys/SaveGame.cpp \
	game/gamesys/SysCmds.cpp \
	game/gamesys/SysCvar.cpp \
	game/gamesys/TypeInfo.cpp \
	game/anim/Anim.cpp \
	game/anim/Anim_Blend.cpp \
	game/anim/Anim_Import.cpp \
	game/anim/Anim_Testmodel.cpp \
	game/script/Script_Compiler.cpp \
	game/script/Script_Interpreter.cpp \
	game/script/Script_Program.cpp \
	game/script/Script_Thread.cpp \
	game/physics/Clip.cpp \
	game/physics/Force.cpp \
	game/physics/Force_Constant.cpp \
	game/physics/Force_Drag.cpp \
	game/physics/Force_Field.cpp \
	game/physics/Force_Spring.cpp \
	game/physics/Physics.cpp \
	game/physics/Physics_AF.cpp \
	game/physics/Physics_Actor.cpp \
	game/physics/Physics_Base.cpp \
	game/physics/Physics_Monster.cpp \
	game/physics/Physics_Parametric.cpp \
	game/physics/Physics_Player.cpp \
	game/physics/Physics_RigidBody.cpp \
	game/physics/Physics_Static.cpp \
	game/physics/Physics_StaticMulti.cpp \
	game/physics/Push.cpp

SRC_SYS := \
	sys/cpu.cpp \
	sys/threads.cpp \
	sys/events.cpp \
	sys/sys_local.cpp \
	sys/posix/posix_net.cpp \
	sys/posix/posix_main.cpp \
	sys/linux/main.cpp \
	sys/glimp.cpp

SRC_TOOLS := \
	tools/edit_stub.cpp \
	tools/debugger/DebuggerBreakpoint.cpp \
	tools/debugger/DebuggerServer.cpp \
	tools/debugger/DebuggerScript.cpp \
	tools/debugger/debugger.cpp

# ---- Assemble (prepend SRCDIR) ----

SRC_ENGINE_CPP := $(SRC_RENDERER) $(SRC_FRAMEWORK) $(SRC_CM) $(SRC_AAS) \
                  $(SRC_SOUND) $(SRC_UI) $(SRC_SYS) $(SRC_TOOLS)

SRC_ENGINE_C := $(SRC_RENDERER_C) $(SRC_FRAMEWORK_C) $(SRC_SOUND_C)

ALL_CPP := $(addprefix $(SRCDIR)/,$(SRC_IDLIB) $(SRC_ENGINE_CPP) $(SRC_GAME))
ALL_C   := $(addprefix $(SRCDIR)/,$(SRC_ENGINE_C))

# ---- Objects ----

OBJ_CPP := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(ALL_CPP))
OBJ_C   := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(ALL_C))
OBJ_ALL := $(OBJ_CPP) $(OBJ_C)
DEPS    := $(OBJ_ALL:.o=.d)

# ============================================================================
# Rules
# ============================================================================

.PHONY: all clean assets assets-clean

all: $(BINARY)

$(BINARY): $(OBJ_ALL)
	@echo "  LINK  $@"
	@$(CXX) $(LDFLAGS) -Wl,-Map,$(BINARY).map -o $@ $^ $(LIBS)
	@echo "  MAP   $(BINARY).map"

# C++ objects
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX   $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# C objects
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# Always-optimized files (stb, miniz — painfully slow in debug)
$(BUILDDIR)/renderer/stblib_impls.o: $(SRCDIR)/renderer/stblib_impls.c
	@mkdir -p $(dir $@)
	@echo "  CC    $< [O2]"
	@$(CC) $(CFLAGS) -O2 -MMD -MP -c -o $@ $<

$(BUILDDIR)/framework/miniz/miniz.o: $(SRCDIR)/framework/miniz/miniz.c
	@mkdir -p $(dir $@)
	@echo "  CC    $< [O2]"
	@$(CC) $(CFLAGS) -O2 -MMD -MP -c -o $@ $<

$(BUILDDIR)/framework/minizip/ioapi.o: $(SRCDIR)/framework/minizip/ioapi.c
	@mkdir -p $(dir $@)
	@echo "  CC    $< [O2]"
	@$(CC) $(CFLAGS) -O2 -MMD -MP -c -o $@ $<

$(BUILDDIR)/framework/minizip/unzip.o: $(SRCDIR)/framework/minizip/unzip.cpp
	@mkdir -p $(dir $@)
	@echo "  CXX   $< [O2]"
	@$(CXX) $(CXXFLAGS) -O2 -MMD -MP -c -o $@ $<

$(BUILDDIR)/sound/stbvorbis_impl.o: $(SRCDIR)/sound/stbvorbis_impl.c
	@mkdir -p $(dir $@)
	@echo "  CC    $< [O2]"
	@$(CC) $(CFLAGS) -O2 -MMD -MP -c -o $@ $<

assets:
	@./tools/assets.sh extract

assets-clean:
	@./tools/assets.sh clean

clean:
	rm -rf $(BUILDDIR) $(BINARY) $(BINARY).map

-include $(DEPS)
