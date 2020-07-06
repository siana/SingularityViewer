# -*- cmake -*-

include(00-Common)

project(cmake)

set(cmake_SOURCE_FILES
    CMakeLists.txt

    00-Common.cmake
    AIStateMachine.cmake
    APR.cmake
    Audio.cmake
    BasicPluginBase.cmake
    Boost.cmake
    BuildBranding.cmake
    BuildVersion.cmake
    CARes.cmake
    CEFPlugin.cmake
    CMakeCopyIfDifferent.cmake
    Colladadom.cmake
    ConfigurePkgConfig.cmake
    CURL.cmake
    Copy3rdPartyLibs.cmake
    CrashPad.cmake
    Cwdebug.cmake
    DBusGlib.cmake
    DeploySharedLibs.cmake
    EXPAT.cmake
    ExamplePlugin.cmake
    FindAPR.cmake
    FindAutobuild.cmake
    FindCARes.cmake
    FindColladadom.cmake
    FindGLOD.cmake
    FindGooglePerfTools.cmake
    FindHunSpell.cmake
    FindNDOF.cmake
    FindOpenJPEG.cmake
    FindTut.cmake
    FindURIPARSER.cmake
    FindXmlRpcEpi.cmake
    FMODSTUDIO.cmake
    FreeType.cmake
    GeneratePrecompiledHeader.cmake
    GLOD.cmake
    GStreamer010Plugin.cmake
    Glui.cmake
    Glut.cmake
    GooglePerfTools.cmake
    Hunspell.cmake
    JPEG.cmake
    LLAddBuildTest.cmake
    LLAppearance.cmake
    LLAudio.cmake
    LLCharacter.cmake
    LLCommon.cmake
    LLImage.cmake
    LLImageJ2COJ.cmake
    LLInventory.cmake
    LLMath.cmake
    LLMessage.cmake
    LLPhysicsExtensions.cmake
    LLPlugin.cmake
    LLPrimitive.cmake
    LLPhysicsExtensions.cmake
    LLRender.cmake
    LLSharedLibs.cmake
    LLUI.cmake
    LLVFS.cmake
    LLWindow.cmake
    LLXML.cmake
    LScript.cmake
    Linking.cmake
    MediaPluginBase.cmake
    NDOF.cmake
    NVAPI.cmake
    OPENAL.cmake
    OpenGL.cmake
    OpenJPEG.cmake
    OpenSSL.cmake
    PNG.cmake
    PluginAPI.cmake
    Prebuilt.cmake
    PulseAudio.cmake
    Python.cmake
    StateMachine.cmake
    TemplateCheck.cmake
    Tut.cmake
    UI.cmake
    UnixInstall.cmake
    URIPARSER.cmake
    Variables.cmake
    ViewerMiscLibs.cmake
    WinManifest.cmake
    LibVLCPlugin.cmake
    XmlRpcEpi.cmake
    ZLIB.cmake
    )

source_group("Shared Rules" FILES ${cmake_SOURCE_FILES})

set(master_SOURCE_FILES
    ../CMakeLists.txt
    )

source_group("Master Rules" FILES ${master_SOURCE_FILES})

set_source_files_properties(${cmake_SOURCE_FILES} ${master_SOURCE_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(cmake
            cmake_dummy.cpp
            ${cmake_SOURCE_FILES}
            ${master_SOURCE_FILES}
            )
