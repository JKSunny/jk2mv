if(NOT InMV)
	message(FATAL_ERROR "Use the top-level cmake script!")
endif(NOT InMV)

if (BuildMVMP)
	#set(MVVulkanRendererDefines "DEFINE001")

	set(MPVulkanRendererIncludeDirectories ${MPDir})
	set(MPVulkanRendererIncludeDirectories
		${MPVulkanRendererIncludeDirectories}
		${MPDir}
		"${MPDir}/rd-vulkan"
		)

	set(MPVulkanRendererFiles
		"${MPDir}/rd-vulkan/G2_API.cpp"
		"${MPDir}/rd-vulkan/G2_bolts.cpp"
		"${MPDir}/rd-vulkan/G2_bones.cpp"
		"${MPDir}/rd-vulkan/G2_misc.cpp"
		"${MPDir}/rd-vulkan/G2_surfaces.cpp"
		"${MPDir}/rd-vulkan/tr_backend.cpp"
		"${MPDir}/rd-vulkan/tr_bsp.cpp"
		"${MPDir}/rd-vulkan/tr_cmds.cpp"
		"${MPDir}/rd-vulkan/tr_curve.cpp"
		"${MPDir}/rd-vulkan/tr_decals.cpp"
		"${MPDir}/rd-vulkan/tr_ghoul2.cpp"
		"${MPDir}/rd-vulkan/tr_image.cpp"
		"${MPDir}/rd-vulkan/tr_init.cpp"
		"${MPDir}/rd-vulkan/tr_light.cpp"
		"${MPDir}/rd-vulkan/tr_local.h"
		"${MPDir}/rd-vulkan/tr_main.cpp"
		"${MPDir}/rd-vulkan/tr_marks.cpp"
		"${MPDir}/rd-vulkan/tr_mesh.cpp"
		"${MPDir}/rd-vulkan/tr_model.cpp"
		"${MPDir}/rd-vulkan/tr_quicksprite.cpp"
		"${MPDir}/rd-vulkan/tr_quicksprite.h"
		"${MPDir}/rd-vulkan/tr_scene.cpp"
		"${MPDir}/rd-vulkan/tr_shade.cpp"
		"${MPDir}/rd-vulkan/tr_shader.cpp"
		"${MPDir}/rd-vulkan/tr_shade_calc.cpp"
		"${MPDir}/rd-vulkan/tr_shadows.cpp"
		"${MPDir}/rd-vulkan/tr_skin.cpp"
		"${MPDir}/rd-vulkan/tr_sky.cpp"
		"${MPDir}/rd-vulkan/tr_subs.cpp"
		"${MPDir}/rd-vulkan/tr_surface.cpp"
		"${MPDir}/rd-vulkan/tr_surfacesprites.cpp"
		"${MPDir}/rd-vulkan/tr_world.cpp"
		"${MPDir}/rd-vulkan/tr_WorldEffects.cpp"
		"${MPDir}/rd-vulkan/tr_WorldEffects.h"
		"${MPDir}/rd-vulkan/vk_attachments.cpp"
		"${MPDir}/rd-vulkan/vk_bloom.cpp"
		"${MPDir}/rd-vulkan/vk_cmd.cpp"
		"${MPDir}/rd-vulkan/vk_debug.cpp"
		"${MPDir}/rd-vulkan/vk_dynamic_glow.cpp"
		"${MPDir}/rd-vulkan/vk_flares.cpp"
		"${MPDir}/rd-vulkan/vk_frame.cpp"
		"${MPDir}/rd-vulkan/vk_image.cpp"
		"${MPDir}/rd-vulkan/vk_image_process.cpp"
		"${MPDir}/rd-vulkan/vk_info.cpp"
		"${MPDir}/rd-vulkan/vk_init.cpp"
		"${MPDir}/rd-vulkan/vk_instance.cpp"
		"${MPDir}/rd-vulkan/vk_local.h"
		"${MPDir}/rd-vulkan/vk_pipelines.cpp"
		"${MPDir}/rd-vulkan/vk_shaders.cpp"
		"${MPDir}/rd-vulkan/vk_shade_geometry.cpp"
		"${MPDir}/rd-vulkan/vk_swapchain.cpp"
		"${MPDir}/rd-vulkan/vk_vbo.cpp"
		"${MPDir}/rd-vulkan/vk_vbo_surfacesprites.cpp"
	)
	source_group("renderer" FILES ${MPVulkanRendererFiles})
		
	set(MPVulkanRendererVulkanAssets
	"${MPDir}/rd-vulkan/vulkan/vk_platform.h"
	"${MPDir}/rd-vulkan/vulkan/vulkan.h"
	"${MPDir}/rd-vulkan/vulkan/vulkan_core.h"
	"${MPDir}/rd-vulkan/vulkan/vulkan_win32.h"
	"${MPDir}/rd-vulkan/vulkan/vulkan_xlib.h"
	"${MPDir}/rd-vulkan/vulkan/vulkan_xlib_xrandr.h")
	source_group("renderer\\vulkan" FILES ${MPVulkanRendererVulkanAssets})
	set(MPVulkanRendererFiles ${MPVulkanRendererFiles} ${MPVulkanRendererVulkanAssets})
	
	set(MPVulkanRendererUtils
	"${MPDir}/rd-vulkan/utils/stb_dxt.h")
	source_group("renderer\\utils" FILES ${MPVulkanRendererUtils})
	set(MPVulkanRendererFiles ${MPVulkanRendererFiles} ${MPVulkanRendererUtils})
	
	
	set(MPVulkanRendererCommon
	"${MPDir}/rd-common/tr_public.h"	
	"${MPDir}/rd-common/tr_common.h"
	"${MPDir}/rd-common/tr_types.h"
	"${MPDir}/rd-common/tr_language.h"
	"${MPDir}/rd-common/tr_font.h"
	"${MPDir}/rd-common/matcomp.h"
	"${MPDir}/rd-common/tr_font.cpp"	
	"${MPDir}/rd-common/tr_image_jpg.cpp"
	"${MPDir}/rd-common/tr_image_png.cpp"
	"${MPDir}/rd-common/tr_image_tga.cpp"
	"${MPDir}/rd-common/tr_image_load.cpp"
	"${MPDir}/rd-common/tr_noise.cpp"
	"${MPDir}/rd-common/matcomp.c")
	source_group("rd-common" FILES ${MPVulkanRendererCommon})
	set(MPVulkanRendererFiles ${MPVulkanRendererFiles} ${MPVulkanRendererCommon})	


	set(MPVulkanRendererCommonFiles
		"${MPDir}/qcommon/qcommon.h"
		"${MPDir}/qcommon/q_shared.h"

		"${MPDir}/qcommon/hstring.cpp"
		"${MPDir}/qcommon/q_math.cpp"
		"${MPDir}/qcommon/q_shared.cpp"
		"${MPDir}/qcommon/sstring.h"
	)
	source_group("qcommon" FILES ${MPVulkanRendererCommonFiles})
	set(MPVulkanRendererFiles ${MPVulkanRendererFiles} ${MPVulkanRendererCommonFiles})

	set(MPVulkanRendererGhoul2Files
		"${MPDir}/ghoul2/G2.h"
		"${MPDir}/ghoul2/G2_local.h"
		"${MPDir}/ghoul2/ghoul2_shared.h"
	)
	source_group("ghoul2" FILES ${MPVulkanRendererGhoul2Files})
	set(MPVulkanRendererFiles ${MPVulkanRendererFiles} ${MPVulkanRendererGhoul2Files})

	#set(MPVulkanRendererOtherFiles
	#	"../../mv_setup.h"
	#)
	#source_group("ghoul2" FILES ${MPVulkanRendererOtherFiles})
	#set(MPVulkanRendererFiles ${MPVulkanRendererFiles} ${MPVulkanRendererOtherFiles})


	if(UseInternalOpenAL)
		# OpenAL
		set(MPVulkanRendererLibraries ${MPVulkanRendererLibraries}
			${LIBDir}/OpenAL/lib/${Architecture}/OpenAL32.lib
		)
		
		set(MPVulkanRendererIncludeDirectories ${MPVulkanRendererIncludeDirectories} "${LIBDir}/OpenAL/include")
	else()
		# OpenAL
		find_package(OpenAL)
		if(NOT OPENAL_FOUND)
			message(FATAL_ERROR "OpenAL dev files not found!")
		endif()
		set(MPVulkanRendererIncludeDirectories ${MPVulkanRendererIncludeDirectories} ${OPENAL_INCLUDE_DIR})
		set(MPVulkanRendererLibraries ${MPVulkanRendererLibraries} ${OPENAL_LIBRARY})
	endif()
	
	# SDL2
	if(UseInternalSDL2)
		set(MPVulkanRendererLibraries ${MPVulkanRendererLibraries}
			${LIBDir}/SDL2/lib/${Architecture}/SDL2main.lib
			${LIBDir}/SDL2/lib/${Architecture}/SDL2.lib
		)
		
		set(MPVulkanRendererIncludeDirectories ${MPVulkanRendererIncludeDirectories} "${LIBDir}/SDL2/include")
	else()
		find_package(SDL2)
		if(NOT SDL2_FOUND)
			message(FATAL_ERROR "libsdl2 dev files not found!")
		endif()
		set(MPVulkanRendererIncludeDirectories ${MPVulkanRendererIncludeDirectories} ${SDL2_INCLUDE_DIR})
		set(MPVulkanRendererLibraries ${MPVulkanRendererLibraries} ${SDL2_LIBRARY})
	endif()

	# EAX
	set(MPVulkanRendererIncludeDirectories ${MPVulkanRendererIncludeDirectories} "${LIBDir}/eax")

	# JPEG
	if(NOT UseInternalJPEG)
		find_package(JPEG)
		if(NOT JPEG_FOUND)
			message(FATAL_ERROR "libjpeg dev files not found! Install it or set UseInternalJPEG ON")
		endif()
	endif()
	set(MPVulkanRendererIncludeDirectories ${MPVulkanRendererIncludeDirectories} ${JPEG_INCLUDE_DIR})
	set(MPVulkanRendererLibraries ${MPVulkanRendererLibraries} ${JPEG_LIBRARIES})

	# PNG
	if(NOT UseInternalPNG)
		find_package(PNG)
		if(NOT PNG_FOUND)
			message(FATAL_ERROR "libpng dev files not found! Install it or set UseInternalPNG ON")
		endif()
	endif()
	set(MPVulkanRendererIncludeDirectories  ${MPVulkanRendererIncludeDirectories} ${PNG_INCLUDE_DIRS})
	set(MPVulkanRendererLibraries ${MPVulkanRendererLibraries} ${PNG_LIBRARIES})

	set(MPVulkanRendererLibraries ${MPVulkanRendererLibraries} ${MINIZIP_LIBRARIES} ${ZLIB_LIBRARIES})


	#set(MPVulkanRendererIncludeDirectories ${MPVulkanRendererIncludeDirectories} ${OpenJKLibDir})
	add_library(${MPVulkanRenderer} SHARED ${MPVulkanRendererFiles})

	set_target_properties(${MPVulkanRenderer} PROPERTIES PREFIX "") 

	target_compile_definitions(${MPVulkanRenderer}
		PRIVATE ${GlobalDefines} ${MVVulkanRendererDefines}
		PRIVATE $<$<CONFIG:Debug>:${DebugDefines}>
		PRIVATE $<$<CONFIG:Release>:${ReleaseDefines}>
		PRIVATE $<$<CONFIG:RelWithDebInfo>:${ReleaseDefines}>
	)

	set_target_properties(${MPVulkanRenderer} PROPERTIES C_STANDARD 90)
	set_target_properties(${MPVulkanRenderer} PROPERTIES INCLUDE_DIRECTORIES "${MPVulkanRendererIncludeDirectories}")
	set_target_properties(${MPVulkanRenderer} PROPERTIES PROJECT_LABEL "jk2mv rd-vulkan")
	target_link_libraries(${MPVulkanRenderer} ${MPVulkanRendererLibraries})

	if(BuildPortableVersion)
		if(WIN32)
			install(TARGETS ${MPVulkanRenderer} RUNTIME DESTINATION ".")
		else()
			install(TARGETS ${MPVulkanRenderer} LIBRARY DESTINATION ".")
		endif()
	else()
		if(WIN32)
			install(TARGETS ${MPVulkanRenderer} RUNTIME DESTINATION ".")
			install(FILES $<TARGET_PDB_FILE:${MPVulkanRenderer}> DESTINATION "." OPTIONAL)
		elseif(APPLE)
			install(TARGETS ${MPVulkanRenderer} LIBRARY DESTINATION "${MVMP}.app/Contents/MacOS")
		else()
			install(TARGETS ${MPVulkanRenderer} LIBRARY DESTINATION "lib")
		endif()
	endif()
endif()
