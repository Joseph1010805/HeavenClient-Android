cmake_minimum_required(VERSION 3.14)
project(HeavenClient)

set(CMAKE_CXX_STANDARD 17)


if(SWITCH_LIBNX)
    add_definitions(-D__SWITCH__=1)
    add_definitions(-DSTATIC_LIBVORBIS=1)
    add_definitions(-DPLATFORM_UNIX=1)
    add_definitions(-DPLATFORM_SWITCH=1)
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_definitions(-D_LARGEFILE_SOURCE=1)
    add_definitions(-DPRAGMA_ONCE=1)
    include_directories("$ENV{DEVKITPRO}/libnx/include")
    add_compile_options(-march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE -mcpu=cortex-a57+crc+fp+simd)
    add_compile_options(-fsigned-char -Wno-strict-aliasing -Wfatal-errors -Wno-sign-compare)
endif()

# ANDROID is set automatically by the NDK's toolchain file.
#
# Android reuses the POSIX paths the Switch port already introduced
# (PLATFORM_UNIX), and adds PLATFORM_ANDROID for the places that differ:
# SDL2 instead of GLFW for windowing/input, GLES2 instead of desktop GL,
# and no romfs - game data lives on external storage.
if(ANDROID)
    add_definitions(-DPLATFORM_UNIX=1)
    add_definitions(-DPLATFORM_ANDROID=1)
    add_definitions(-D_FILE_OFFSET_BITS=64)
    add_definitions(-D_LARGEFILE_SOURCE=1)
    add_definitions(-DPRAGMA_ONCE=1)

    # Net/Session.h picks a socket backend at compile time. The Switch build
    # falls through to mbedtls; Android has a normal POSIX stack, so it uses
    # asio like the desktop build and mbedtls is dropped entirely.
    add_definitions(-DUSE_ASIO=1)
    add_definitions(-DASIO_STANDALONE=1)

    add_compile_options(-fsigned-char -Wno-strict-aliasing -Wno-sign-compare)

    # Shared client code brace-initialises Point<int16_t> from int arithmetic
    # (Template/Point.h). MSVC and GCC allow the narrowing; clang makes it a
    # hard error. Demoting it keeps the port from touching code every other
    # platform shares - the truncation to int16_t coordinates is deliberate.
    add_compile_options(-Wno-c++11-narrowing)

    # Template/EnumMap.h derives from std::iterator, deprecated in C++17.
    add_compile_options(-Wno-deprecated-declarations)
endif()

include_directories(.)
#include_directories(Audio)
include_directories(Character)
include_directories(Character/Inventory)
include_directories(Character/Look)
include_directories(Data)
include_directories(Gameplay)
include_directories(Gameplay/Combat)
include_directories(Gameplay/MapleMap)
include_directories(Gameplay/Physics)
include_directories(Graphics)
include_directories(IO)
include_directories(IO/Components)
include_directories(IO/UITypes)
include_directories(Net)
include_directories(Net/Handlers)
include_directories(Net/Handlers/Helpers)
include_directories(Net/Packets)
include_directories(Template)
include_directories(Util)

set(HEAVEN_SOURCES
        Audio/Audio.cpp
        Audio/Audio.h
        Character/Inventory/Equip.cpp
        Character/Inventory/Equip.h
        Character/Inventory/EquipQuality.cpp
        Character/Inventory/EquipQuality.h
        Character/Inventory/Inventory.cpp
        Character/Inventory/Inventory.h
        Character/Inventory/InventoryType.cpp
        Character/Inventory/InventoryType.h
        Character/Inventory/Item.cpp
        Character/Inventory/Item.h
        Character/Inventory/Pet.cpp
        Character/Inventory/Pet.h
        Character/Inventory/Weapon.cpp
        Character/Inventory/Weapon.h
        Character/Look/Afterimage.cpp
        Character/Look/Afterimage.h
        Character/Look/Body.cpp
        Character/Look/Body.h
        Character/Look/BodyDrawinfo.cpp
        Character/Look/BodyDrawinfo.h
        Character/Look/CharEquips.cpp
        Character/Look/CharEquips.h
        Character/Look/CharLook.cpp
        Character/Look/CharLook.h
        Character/Look/Clothing.cpp
        Character/Look/Clothing.h
        Character/Look/Equipslot.cpp
        Character/Look/Equipslot.h
        Character/Look/Face.cpp
        Character/Look/Face.h
        Character/Look/Hair.cpp
        Character/Look/Hair.h
        Character/Look/PetLook.cpp
        Character/Look/PetLook.h
        Character/Look/Stance.cpp
        Character/Look/Stance.h
        Character/ActiveBuffs.cpp
        Character/ActiveBuffs.h
        Character/Buff.cpp
        Character/Buff.h
        Character/Char.cpp
        Character/Char.h
        Character/CharEffect.cpp
        Character/CharEffect.h
        Character/CharStats.cpp
        Character/CharStats.h
        Character/Equipstat.cpp
        Character/Equipstat.h
        Character/ExpTable.cpp
        Character/ExpTable.h
        Character/Job.cpp
        Character/Job.h
        Character/Maplestat.cpp
        Character/Maplestat.h
        Character/Monsterbook.cpp
        Character/Monsterbook.h
        Character/OtherChar.cpp
        Character/OtherChar.h
        Character/PassiveBuffs.cpp
        Character/PassiveBuffs.h
        Character/Party.cpp
        Character/Party.h
        Character/Player.cpp
        Character/Player.h
        Character/PlayerStates.cpp
        Character/PlayerStates.h
        Character/Questlog.cpp
        Character/Questlog.h
        Character/Skillbook.cpp
        Character/Skillbook.h
        Character/SkillId.h
        Character/StatCaps.h
        Character/Telerock.cpp
        Character/Telerock.h
        Data/BulletData.cpp
        Data/BulletData.h
        Data/EquipData.cpp
        Data/EquipData.h
        Data/ItemData.cpp
        Data/ItemData.h
        Data/JobData.cpp
        Data/JobData.h
        Data/SkillData.cpp
        Data/SkillData.h
        Data/WeaponData.cpp
        Data/WeaponData.h
        Gameplay/Combat/Attack.h
        Gameplay/Combat/Bullet.cpp
        Gameplay/Combat/Bullet.h
        Gameplay/Combat/Combat.cpp
        Gameplay/Combat/Combat.h
        Gameplay/Combat/DamageNumber.cpp
        Gameplay/Combat/DamageNumber.h
        Gameplay/Combat/RegularAttack.cpp
        Gameplay/Combat/RegularAttack.h
        Gameplay/Combat/Skill.cpp
        Gameplay/Combat/Skill.h
        Gameplay/Combat/SkillAction.cpp
        Gameplay/Combat/SkillAction.h
        Gameplay/Combat/SkillBullet.cpp
        Gameplay/Combat/SkillBullet.h
        Gameplay/Combat/SkillHitEffect.cpp
        Gameplay/Combat/SkillHitEffect.h
        Gameplay/Combat/SkillSound.cpp
        Gameplay/Combat/SkillSound.h
        Gameplay/Combat/SkillUseEffect.cpp
        Gameplay/Combat/SkillUseEffect.h
        Gameplay/Combat/SpecialMove.h
        Gameplay/MapleMap/Drop.cpp
        Gameplay/MapleMap/Drop.h
        Gameplay/MapleMap/ItemDrop.cpp
        Gameplay/MapleMap/ItemDrop.h
        Gameplay/MapleMap/Layer.h
        Gameplay/MapleMap/MapBackgrounds.cpp
        Gameplay/MapleMap/MapBackgrounds.h
        Gameplay/MapleMap/MapChars.cpp
        Gameplay/MapleMap/MapChars.h
        Gameplay/MapleMap/MapDrops.cpp
        Gameplay/MapleMap/MapDrops.h
        Gameplay/MapleMap/MapEffect.cpp
        Gameplay/MapleMap/MapEffect.h
        Gameplay/MapleMap/MapInfo.cpp
        Gameplay/MapleMap/MapInfo.h
        Gameplay/MapleMap/MapMobs.cpp
        Gameplay/MapleMap/MapMobs.h
        Gameplay/MapleMap/MapNpcs.cpp
        Gameplay/MapleMap/MapNpcs.h
        Gameplay/MapleMap/MapObject.cpp
        Gameplay/MapleMap/MapObject.h
        Gameplay/MapleMap/MapObjects.cpp
        Gameplay/MapleMap/MapObjects.h
        Gameplay/MapleMap/MapPortals.cpp
        Gameplay/MapleMap/MapPortals.h
        Gameplay/MapleMap/Door.cpp
        Gameplay/MapleMap/Door.h
        Gameplay/MapleMap/MapDoors.cpp
        Gameplay/MapleMap/MapDoors.h
        Gameplay/MapleMap/MapMists.cpp
        Gameplay/MapleMap/MapMists.h
        Gameplay/MapleMap/MapSummons.cpp
        Gameplay/MapleMap/MapSummons.h
        Gameplay/MapleMap/Mist.cpp
        Gameplay/MapleMap/Mist.h
        Gameplay/MapleMap/Summon.cpp
        Gameplay/MapleMap/Summon.h
        Gameplay/MapleMap/MapReactors.cpp
        Gameplay/MapleMap/MapReactors.h
        Gameplay/MapleMap/MapTilesObjs.cpp
        Gameplay/MapleMap/MapTilesObjs.h
        Gameplay/MapleMap/MesoDrop.cpp
        Gameplay/MapleMap/MesoDrop.h
        Gameplay/MapleMap/Mob.cpp
        Gameplay/MapleMap/Mob.h
        Gameplay/MapleMap/Npc.cpp
        Gameplay/MapleMap/Npc.h
        Gameplay/MapleMap/Obj.cpp
        Gameplay/MapleMap/Obj.h
        Gameplay/MapleMap/Portal.cpp
        Gameplay/MapleMap/Portal.h
        Gameplay/MapleMap/Reactor.cpp
        Gameplay/MapleMap/Reactor.h
        Gameplay/MapleMap/Tile.cpp
        Gameplay/MapleMap/Tile.h
        Gameplay/Physics/Foothold.cpp
        Gameplay/Physics/Foothold.h
        Gameplay/Physics/Footholdtree.cpp
        Gameplay/Physics/Footholdtree.h
        Gameplay/Physics/Physics.cpp
        Gameplay/Physics/Physics.h
        Gameplay/Physics/PhysicsObject.h
        Gameplay/Camera.cpp
        Gameplay/Camera.h
        Gameplay/Movement.h
        Gameplay/Playable.h
        Gameplay/Spawn.cpp
        Gameplay/Spawn.h
        Gameplay/Stage.cpp
        Gameplay/Stage.h
        Graphics/Animation.cpp
        Graphics/Animation.h
        Graphics/Color.cpp
        Graphics/Color.h
        Graphics/DrawArgument.h
        Graphics/EffectLayer.cpp
        Graphics/EffectLayer.h
        Graphics/Geometry.cpp
        Graphics/Geometry.h
        Graphics/GraphicsGL.cpp
        Graphics/GraphicsGL.h
        Graphics/SpecialText.h
        Graphics/Sprite.cpp
        Graphics/Sprite.h
        Graphics/Text.cpp
        Graphics/Text.h
        Graphics/Texture.cpp
        Graphics/Texture.h
        IO/Components/AreaButton.cpp
        IO/Components/AreaButton.h
        IO/Components/Button.cpp
        IO/Components/Button.h
        IO/Components/Charset.cpp
        IO/Components/Charset.h
        IO/Components/ChatBalloon.cpp
        IO/Components/ChatBalloon.h
        IO/Components/EquipTooltip.cpp
        IO/Components/EquipTooltip.h
        IO/Components/Gauge.cpp
        IO/Components/Gauge.h
        IO/Components/Icon.cpp
        IO/Components/Icon.h
        IO/Components/IconCover.cpp
        IO/Components/IconCover.h
        IO/Components/ItemTooltip.cpp
        IO/Components/ItemTooltip.h
        IO/Components/MapleButton.cpp
        IO/Components/MapleButton.h
        IO/Components/MapleComboBox.cpp
        IO/Components/MapleComboBox.h
        IO/Components/MapleFrame.cpp
        IO/Components/MapleFrame.h
        IO/Components/MapTooltip.cpp
        IO/Components/MapTooltip.h
        IO/Components/Nametag.cpp
        IO/Components/Nametag.h
        IO/Components/Npctext.cpp
        IO/Components/ScrollingNotice.cpp
        IO/Components/ScrollingNotice.h
        IO/Components/SkillTooltip.cpp
        IO/Components/SkillTooltip.h
        IO/Components/Slider.cpp
        IO/Components/Slider.h
        IO/Components/StatefulIcon.cpp
        IO/Components/StatefulIcon.h
        IO/Components/Textfield.cpp
        IO/Components/Textfield.h
        IO/Components/TextTooltip.cpp
        IO/Components/TextTooltip.h
        IO/Components/Tooltip.h
        IO/Components/TwoSpriteButton.cpp
        IO/Components/TwoSpriteButton.h
        IO/UITypes/UIAranCreation.cpp
        IO/UITypes/UIAranCreation.h
        IO/UITypes/UIBuffList.cpp
        IO/UITypes/UIBuffList.h
        IO/UITypes/UICashShop.cpp
        IO/UITypes/UICashShop.h
        IO/UITypes/UIChannel.cpp
        IO/UITypes/UIChannel.h
        IO/UITypes/UICharSelect.cpp
        IO/UITypes/UICharSelect.h
        IO/UITypes/UIChat.cpp
        IO/UITypes/UIChat.h
        IO/UITypes/UIChatbar.cpp
        IO/UITypes/UIChatbar.h
        IO/UITypes/UICygnusCreation.cpp
        IO/UITypes/UICygnusCreation.h
        IO/UITypes/UIEquipInventory.cpp
        IO/UITypes/UIEquipInventory.h
        IO/UITypes/UIEvent.cpp
        IO/UITypes/UIEvent.h
        IO/UITypes/UIExplorerCreation.cpp
        IO/UITypes/UIExplorerCreation.h
        IO/UITypes/UIGender.cpp
        IO/UITypes/UIGender.h
        IO/UITypes/UIItemInventory.cpp
        IO/UITypes/UIItemInventory.h
        IO/UITypes/UIJoypad.cpp
        IO/UITypes/UIJoypad.h
        IO/UITypes/UIKeyConfig.cpp
        IO/UITypes/UIKeyConfig.h
        IO/UITypes/UILogin.cpp
        IO/UITypes/UILogin.h
        IO/UITypes/UILoginNotice.cpp
        IO/UITypes/UILoginNotice.h
        IO/UITypes/UILoginwait.cpp
        IO/UITypes/UILoginwait.h
        IO/UITypes/UILogo.cpp
        IO/UITypes/UILogo.h
        IO/UITypes/UIMiniMap.cpp
        IO/UITypes/UIMiniMap.h
        IO/UITypes/UINotice.cpp
        IO/UITypes/UINotice.h
        IO/UITypes/UIPartyHUD.cpp
        IO/UITypes/UIPartyHUD.h
        IO/UITypes/UINpcTalk.cpp
        IO/UITypes/UINpcTalk.h
        IO/UITypes/UIOptionMenu.cpp
        IO/UITypes/UIOptionMenu.h
        IO/UITypes/UIQuestLog.cpp
        IO/UITypes/UIQuestLog.h
        IO/UITypes/UIQuit.cpp
        IO/UITypes/UIQuit.h
        IO/UITypes/UIRaceSelect.cpp
        IO/UITypes/UIRaceSelect.h
        IO/UITypes/UIRegion.cpp
        IO/UITypes/UIRegion.h
        IO/UITypes/UIShop.cpp
        IO/UITypes/UIShop.h
        IO/UITypes/UISkillbook.cpp
        IO/UITypes/UISkillbook.h
        IO/UITypes/UISoftKey.cpp
        IO/UITypes/UISoftKey.h
        IO/UITypes/UIStatsinfo.cpp
        IO/UITypes/UIStatsinfo.h
        IO/UITypes/UIStatusbar.cpp
        IO/UITypes/UIStatusbar.h
        IO/UITypes/UIStatusMessenger.cpp
        IO/UITypes/UIStatusMessenger.h
        IO/UITypes/UITermsOfService.cpp
        IO/UITypes/UITermsOfService.h
        IO/UITypes/UIUserList.cpp
        IO/UITypes/UIUserList.h
        IO/UITypes/UIWorldMap.cpp
        IO/UITypes/UIWorldMap.h
        IO/UITypes/UIWorldSelect.cpp
        IO/UITypes/UIWorldSelect.h
        IO/Cursor.cpp
        IO/Cursor.h
        IO/KeyAction.h
        IO/Keyboard.cpp
        IO/Keyboard.h
        IO/KeyConfig.h
        IO/KeyType.h
        IO/Messages.cpp
        IO/Messages.h
        IO/UI.cpp
        IO/UI.h
        IO/UIDragElement.h
        IO/UIElement.cpp
        IO/UIElement.h
        IO/UIState.h
        IO/UIStateCashShop.cpp
        IO/UIStateCashShop.h
        IO/UIStateGame.cpp
        IO/UIStateGame.h
        IO/UIStateLogin.cpp
        IO/UIStateLogin.h
        IO/Window.cpp
        IO/Window.h
        Net/Handlers/Helpers/ItemParser.cpp
        Net/Handlers/Helpers/ItemParser.h
        Net/Handlers/Helpers/LoginParser.cpp
        Net/Handlers/Helpers/LoginParser.h
        Net/Handlers/Helpers/MovementParser.cpp
        Net/Handlers/Helpers/MovementParser.h
        Net/Handlers/AttackHandlers.cpp
        Net/Handlers/AttackHandlers.h
        Net/Handlers/CashShopHandlers.cpp
        Net/Handlers/CashShopHandlers.h
        Net/Handlers/Helpers/CashShopParser.cpp
        Net/Handlers/Helpers/CashShopParser.h
        Net/Handlers/CommonHandlers.cpp
        Net/Handlers/CommonHandlers.h
        Net/Handlers/CustomHandlers.cpp
        Net/Handlers/CustomHandlers.h
        Net/Handlers/InventoryHandlers.cpp
        Net/Handlers/InventoryHandlers.h
        Net/Handlers/LoginHandlers.cpp
        Net/Handlers/LoginHandlers.h
        Net/Handlers/MapObjectHandlers.cpp
        Net/Handlers/MapObjectHandlers.h
        Net/Handlers/MessagingHandlers.cpp
        Net/Handlers/MessagingHandlers.h
        Net/Handlers/NpcInteractionHandlers.cpp
        Net/Handlers/NpcInteractionHandlers.h
        Net/Handlers/SocialHandlers.cpp
        Net/Handlers/SocialHandlers.h
        Net/Handlers/PlayerHandlers.cpp
        Net/Handlers/PlayerHandlers.h
        Net/Handlers/SetfieldHandlers.cpp
        Net/Handlers/SetfieldHandlers.h
        Net/Handlers/TestingHandlers.cpp
        Net/Handlers/TestingHandlers.h
        Net/Packets/AttackAndSkillPackets.h
        Net/Packets/CharCreationPackets.h
        Net/Packets/CommonPackets.h
        Net/Packets/CustomPackets.h
        Net/Packets/GameplayPackets.h
        Net/Packets/InventoryPackets.h
        Net/Packets/LoginPackets.h
        Net/Packets/MessagingPackets.h
        Net/Packets/MovementPacket.h
        Net/Packets/NpcInteractionPackets.h
        Net/Packets/PlayerPackets.h
        Net/Packets/SelectCharPackets.h
        Net/Cryptography.cpp
        Net/Cryptography.h
        Net/InPacket.cpp
        Net/InPacket.h
        Net/Login.h
        Net/NetConstants.h
        Net/OutPacket.cpp
        Net/OutPacket.h
        Net/PacketError.h
        Net/PacketHandler.h
        Net/PacketSwitch.cpp
        Net/PacketSwitch.h
        Net/Session.cpp
        Net/Session.h
        Net/SocketAsio.cpp
        Net/SocketAsio.h
        Net/SocketMbedtls.cpp
        Net/SocketMbedtls.h
        Net/SocketWinsock.cpp
        Net/SocketWinsock.h
        Template/BoolPair.h
        Template/Cache.h
        Template/Enumeration.h
        Template/EnumMap.h
        Template/Interpolated.h
        Template/Optional.h
        Template/Point.h
        Template/Range.h
        Template/Rectangle.h
        Template/Singleton.h
        Template/TimedQueue.h
        Template/TypeMap.h
        Util/HardwareInfo.h
        Util/HashUtility.cpp
        Util/HashUtility.h
        Util/Lerp.h
        Util/Misc.cpp
        Util/Misc.h
        Util/NxFiles.cpp
        Util/NxFiles.h
        Util/QuadTree.h
        Util/Randomizer.h
        Util/ScreenResolution.h
        Util/Silent.cpp
        Util/Silent.h
        Util/TimedBool.h
        Configuration.cpp
        Configuration.h
        Console.h
        Constants.h
        Error.h
        MapleStory.cpp
        MapleStory.h
        resource.h
        Timer.h
        Audio/membuf.cpp
        Audio/membuf.h)

# Android loads native code as a shared library from the Java shell, so the
# target is a .so rather than an executable. switch_wrapper.c is libnx-only
# (romfs + nxlink) and has no Android equivalent.
# Vendored dependencies, carried as git submodules (see .gitmodules).
# The Switch build takes these from devkitPro portlibs instead.
if(ANDROID)
    set(SDL_SHARED ON  CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    add_subdirectory(libs/SDL2   EXCLUDE_FROM_ALL)
    add_subdirectory(libs/freetype EXCLUDE_FROM_ALL)

    # Audio: the same stack the Switch port uses. OpenAL Soft already has an
    # OpenSL ES backend (Alc/backends/opensl.c), which is Android's audio path,
    # so no backend work is needed - only turning off the parts we don't ship.
    set(ALSOFT_EXAMPLES  OFF CACHE BOOL "" FORCE)
    set(ALSOFT_UTILS     OFF CACHE BOOL "" FORCE)
    set(ALSOFT_TESTS     OFF CACHE BOOL "" FORCE)
    # Must stay ON. openal-soft only puts its OpenAL target into an export set
    # when installing, and alure2 has an unconditional export() that requires
    # every public dependency to be exported. Turning this off fails the
    # generate step with "target OpenAL is not in any export set". We never run
    # install, so the rules themselves are inert.
    set(ALSOFT_INSTALL   ON  CACHE BOOL "" FORCE)
    add_subdirectory(libs/openal-soft EXCLUDE_FROM_ALL)

    set(ALURE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ALURE_BUILD_STATIC   ON  CACHE BOOL "" FORCE)

    # alure2 does find_package(OpenAL REQUIRED), which hunts for a system
    # install and cannot see the copy we are building in this same project.
    # Pre-seeding the cache variables makes find_package succeed and point at
    # our in-tree build - OpenAL is openal-soft's target name.
    set(OPENAL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/libs/openal-soft/include
        CACHE PATH "" FORCE)
    set(OPENAL_LIBRARY OpenAL CACHE STRING "" FORCE)

    add_subdirectory(libs/alure EXCLUDE_FROM_ALL)
endif()

if(ANDROID)
    # IO/Window.cpp is GLFW-based and GLFW has no Android backend.
    # IO/Window_Android.cpp is the SDL2 equivalent - same class, same contract.
    list(REMOVE_ITEM HEAVEN_SOURCES IO/Window.cpp)
    list(APPEND HEAVEN_SOURCES IO/Window_Android.cpp)

    # The lower panel on a handheld that has one. Android only - it needs a
    # Surface handed down from Java.
    list(APPEND HEAVEN_SOURCES IO/SecondScreen_Android.cpp)
    list(APPEND HEAVEN_SOURCES IO/SecondScreenPanel.cpp)

    # mbedtls backend is Switch-only - see USE_ASIO above.
    list(REMOVE_ITEM HEAVEN_SOURCES Net/SocketMbedtls.cpp)

    # NoLifeNx reads the WZ/NX game data. Its own CMakeLists links a prebuilt
    # liblz4.a that only exists for the Switch toolchain, so its sources and
    # the bundled lz4 are compiled straight into the client instead. The code
    # itself is already portable - it has POSIX mmap paths alongside Win32.
    list(APPEND HEAVEN_SOURCES
        libs/NoLifeNx/nlnx/audio.cpp
        libs/NoLifeNx/nlnx/bitmap.cpp
        libs/NoLifeNx/nlnx/file.cpp
        libs/NoLifeNx/nlnx/node.cpp
        libs/NoLifeNx/nlnx/nx.cpp
        libs/NoLifeNx/nlnx/lz4-1.8.2/lib/lz4.c
        libs/NoLifeNx/nlnx/lz4-1.8.2/lib/lz4frame.c
        libs/NoLifeNx/nlnx/lz4-1.8.2/lib/lz4hc.c
        libs/NoLifeNx/nlnx/lz4-1.8.2/lib/xxhash.c)
    add_library(HeavenClient SHARED ${HEAVEN_SOURCES})
else()
    list(APPEND HEAVEN_SOURCES switch_wrapper.c)
    add_executable(HeavenClient ${HEAVEN_SOURCES})
endif()

if(ANDROID)
    find_library(ANDROID_LOG_LIB log)
    find_library(ANDROID_EGL_LIB EGL)
    # GLESv3 rather than GLESv2: the renderer is GLES2, but the frame is
    # upscaled to the panel with glBlitFramebuffer, which is ES3. libGLESv3
    # exports the ES2 entry points too, so this is a superset.
    find_library(ANDROID_GLES3_LIB GLESv3)

    target_link_libraries(HeavenClient ${ANDROID_EGL_LIB})
    target_link_libraries(HeavenClient ${ANDROID_GLES3_LIB})
    target_link_libraries(HeavenClient ${ANDROID_LOG_LIB})
    target_link_libraries(HeavenClient android)

    target_link_libraries(HeavenClient SDL2)
    target_link_libraries(HeavenClient freetype)
    target_link_libraries(HeavenClient OpenAL)
    # alure builds its static library as alure2_s, not alure2.
    target_link_libraries(HeavenClient alure2_s)
    target_link_libraries(HeavenClient OpenSLES)

    # Note the doubled path: at tag asio-1-12-2 the headers sit under
    # asio/include, not include. Newer asio moved them.
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/asio/asio/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/openal-soft/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/alure/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/SDL2/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/freetype/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/NoLifeNx)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/NoLifeNx/nlnx/lz4-1.8.2/lib)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/stb)
endif()

if(SWITCH_LIBNX)
    target_link_libraries(HeavenClient ${PROJECT_SOURCE_DIR}/libs/alure/switch-build/libalure2_s.a)
    target_link_libraries(HeavenClient ${PROJECT_SOURCE_DIR}/libs/glad/lib/libglad.a)
    target_link_libraries(HeavenClient "EGL")
    target_link_libraries(HeavenClient "glapi")
    target_link_libraries(HeavenClient "drm_nouveau")
    target_link_libraries(HeavenClient "pthread")
    target_link_libraries(HeavenClient "nx")
    target_link_libraries(HeavenClient freetype)
    target_link_libraries(HeavenClient bz2)
    target_link_libraries(HeavenClient png)
    target_link_libraries(HeavenClient "m")
    target_link_libraries(HeavenClient "z")
    target_link_libraries(HeavenClient "mbedtls")
    target_link_libraries(HeavenClient "mbedcrypto")
    target_link_libraries(HeavenClient ${PROJECT_SOURCE_DIR}/libs/openal-soft/lib/libopenal.a)
    target_link_libraries(HeavenClient "SDL2")
    target_link_libraries(HeavenClient ${PROJECT_SOURCE_DIR}/libs/glfw/build/src/libglfw3.a)
    target_link_libraries(HeavenClient ${PROJECT_SOURCE_DIR}/libs/NoLifeNx/nlnx/lz4-1.8.2/lib/liblz4.a)
    target_link_libraries(HeavenClient ${PROJECT_SOURCE_DIR}/libs/NoLifeNx/nlnx/switch-build/libNoLifeNx.a)

    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/glad/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC /home/rich/HeavenClient/glfw/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC /opt/devkitpro/portlibs/switch/include/GL)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC /home/rich/HeavenClient/HeavenClient/asio-1.12.2/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC /opt/devkitpro/portlibs/switch/include/freetype2)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC /opt/devkitpro/portlibs/switch/include/mbedtls)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/openal-soft/include)
    #TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC /opt/devkitpro/portlibs/switch/include)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/NoLifeNx)
    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/alure/include)

    TARGET_INCLUDE_DIRECTORIES(HeavenClient PUBLIC ${PROJECT_SOURCE_DIR}/libs/stb)
endif()

include_directories("$ENV{PORTLIBS_PREFIX}/include")

# Android has no separate libpthread - the pthread API lives in libc.
if(NOT ANDROID)
    target_link_libraries(HeavenClient pthread)
endif()

set(BINARY_NAME "HeavenClient")
set(NRONAME "HeavenClient")
set(GAMENAME "HeavenClient")

# Packages the binary as a Switch .nro. nacptool/elf2nro are devkitPro tools
# and do not exist on any other platform, so this must not run elsewhere -
# Gradle packages the .so into an APK itself.
if(NOT ANDROID)
    add_custom_command(TARGET HeavenClient POST_BUILD
            COMMAND nacptool --create "${GAMENAME}" "rich" "1.0.0-nx" "${CMAKE_BINARY_DIR}/${NRONAME}.nacp"
            COMMAND aarch64-none-elf-strip -o "${CMAKE_BINARY_DIR}/${BINARY_NAME}_stripped" "${CMAKE_BINARY_DIR}/${BINARY_NAME}"
            COMMAND elf2nro "${CMAKE_BINARY_DIR}/${BINARY_NAME}" "${CMAKE_BINARY_DIR}/${NRONAME}.nro" --icon="${CMAKE_SOURCE_DIR}/ms_icon.jpg" --nacp="${CMAKE_BINARY_DIR}/${NRONAME}.nacp"
            )
endif()
