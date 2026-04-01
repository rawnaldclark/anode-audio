# ProGuard rules for GuitarEmulator

# Keep all native method declarations - required for JNI bridge to work correctly.
# Without this, R8 may rename or remove classes/methods referenced from C++ code.
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep the AudioEngine class entirely since it is the JNI bridge target.
# The C++ jni_bridge.cpp references this class and its methods by name.
-keep class com.guitaremulator.app.audio.AudioEngine {
    *;
}

# Keep the Application class referenced in AndroidManifest.xml
-keep class com.guitaremulator.app.GuitarEmulatorApp {
    *;
}

# Keep the foreground service class referenced in AndroidManifest.xml
-keep class com.guitaremulator.app.audio.AudioProcessingService {
    *;
}
