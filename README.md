# MediaPlayer
ffmpeg
How to Build ffmpeg with NDK 

1. Download ffmpeg source code

FFMPEG source code can be downloaded from the ffmpeg website. The latest stable release is 2.0.1. Download the source code and decompress it to $NDK/sources folder. We’ll discuss about the reason for doing this later.

2. Update configure file

Open ffmpeg-2.xx/configure file with a text editor, and locate the following lines.

 >   SLIBNAME_WITH_MAJOR='\$(SLIBNAME).\$(LIBMAJOR)'
    LIB_INSTALL_EXTRA_CMD='$$(RANLIB) "$(LIBDIR)/$(LIBNAME)"'
    SLIB_INSTALL_NAME='$(SLIBNAME_WITH_VERSION)'
    SLIB_INSTALL_LINKS='$(SLIBNAME_WITH_MAJOR) $(SLIBNAME)'

This cause ffmpeg shared libraries to be compiled to libavcodec.so.<version> (e.g. libavcodec.so.55), which is not compatible with Android build system. Therefore we’ll need to replace the above lines with the following lines.

 >   SLIBNAME_WITH_MAJOR='\$(SLIBPREF)\$(FULLNAME)-\$(LIBMAJOR)\$(SLIBSUF)'
    LIB_INSTALL_EXTRA_CMD='$$(RANLIB) "$(LIBDIR)/$(LIBNAME)"'
    SLIB_INSTALL_NAME='$(SLIBNAME_WITH_MAJOR)'
    SLIB_INSTALL_LINKS='$(SLIBNAME)'
    

3. Build ffmpeg

Copy the following text to a text editor and save it as build_android.sh.

    #!/bin/bash
    NDK=$HOME/Desktop/adt/android-ndk-r9
    SYSROOT=$NDK/platforms/android-9/arch-arm/
    
    #[如果是mac的话就是 darwin-x86_64]
    TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86_64  
    
    function build_one
    {
    ./configure \
        --prefix=$PREFIX \
        --enable-shared \
        --disable-static \
        --disable-doc \
        --disable-ffmpeg \
        --disable-ffplay \
        --disable-ffprobe \
        --disable-ffserver \
        --disable-avdevice \
        --disable-doc \
        --disable-symver \
        --cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
        --target-os=linux \
        --arch=arm \
        --enable-cross-compile \
        --sysroot=$SYSROOT \
        --extra-cflags="-Os -fpic $ADDI_CFLAGS" \
        --extra-ldflags="$ADDI_LDFLAGS" \
        $ADDITIONAL_CONFIGURE_FLAG
    make clean
    make
    make install
    }
    CPU=arm
    PREFIX=$(pwd)/android/$CPU 
    ADDI_CFLAGS="-marm"
    build_one


We disabled static library and enabled shared library. Note that the build script is not optimized for a particular CPU. One should refer to ffmpeg documentation for detailed information about available configure options.

Once the file is saved, make sure the script is executable by the command below,

>sudo chmod +x build_android.sh

Then execute the script by the command,

>./build_android.sh

4. Build Output

The build can take a while to finish depending on your computer speed. Once it’s done, you should be able to find a folder $NDK/sources/ffmpeg-2.0.1/android, which contains arm/lib and arm/include folders.

The arm/lib folder contains the shared libraries, while arm/include folder contains the header files for libavcodec, libavformat, libavfilter, libavutil, libswscale etc.

Note that the arm/lib folder contains both the library files (e.g.: libavcodec-55.so) and symbolic links (e.g.: libavcodec.so) to them. We can remove the symbolic links to avoid confusion.

5. Make ffmpeg Libraries available for Your Projects

Now we’ve compiled the ffmpeg libraries and ready to use them. Android NDK allows us to reuse a compiled module through the import-module build command.

The reason we built our ffmpeg source code under $NDK/sources folder is that NDK build system will search for directories under this path for external modules automatically. To declare the ffmpeg libraries as reusable modules, we’ll need to add a file named $NDK/sources/ffmpeg-2.0.1/android/arm/Android.mk with the following content,

    LOCAL_PATH:= $(call my-dir)
     
    include $(CLEAR_VARS)
    LOCAL_MODULE:= libavcodec
    LOCAL_SRC_FILES:= lib/libavcodec-55.so
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
    include $(PREBUILT_SHARED_LIBRARY)
     
    include $(CLEAR_VARS)
    LOCAL_MODULE:= libavformat
    LOCAL_SRC_FILES:= lib/libavformat-55.so
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
    include $(PREBUILT_SHARED_LIBRARY)
     
    include $(CLEAR_VARS)
    LOCAL_MODULE:= libswscale
    LOCAL_SRC_FILES:= lib/libswscale-2.so
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
    include $(PREBUILT_SHARED_LIBRARY)
     
    include $(CLEAR_VARS)
    LOCAL_MODULE:= libavutil
    LOCAL_SRC_FILES:= lib/libavutil-52.so
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
    include $(PREBUILT_SHARED_LIBRARY)
     
    include $(CLEAR_VARS)
    LOCAL_MODULE:= libavfilter
    LOCAL_SRC_FILES:= lib/libavfilter-3.so
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
    include $(PREBUILT_SHARED_LIBRARY)
     
    include $(CLEAR_VARS)
    LOCAL_MODULE:= libwsresample
    LOCAL_SRC_FILES:= lib/libswresample-0.so
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
    include $(PREBUILT_SHARED_LIBRARY)
    Below is an example of how we can use the libraries in a Android project’s jni/Android.mk file,
    
    LOCAL_PATH := $(call my-dir)
     
    include $(CLEAR_VARS)
 
    LOCAL_MODULE    := tutorial03
    LOCAL_SRC_FILES := tutorial03.c
    LOCAL_LDLIBS := -llog -ljnigraphics -lz -landroid
    LOCAL_SHARED_LIBRARIES := libavformat libavcodec libswscale libavutil
 
    include $(BUILD_SHARED_LIBRARY)
    $(call import-module,ffmpeg-2.0.1/android/arm)

Note that we called import-module with the relative path to $NDK/sources for the build system to locate the reusable ffmpeg libraries.

For real examples to how to use the ffmpeg libraries in Android app, please refer to my github repo of android-ffmpeg-tutorial.


[转载@http://www.roman10.net/how-to-build-ffmpeg-with-ndk-r9/][1]


[1]: http://www.roman10.net/how-to-build-ffmpeg-with-ndk-r9/



[http://www.roman10.net/how-to-build-ffmpeg-with-ndk-r9/]: http://www.roman10.net/how-to-build-ffmpeg-with-ndk-r9/
