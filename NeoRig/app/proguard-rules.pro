# Правила R8 (используются, только если включить isMinifyEnabled = true).

# kotlinx.serialization: сохраняем сгенерированные сериализаторы моделей
-keepattributes *Annotation*, InnerClasses
-keepclassmembers class com.neorig.pcbuilder.** {
    *** Companion;
}
-keepclasseswithmembers class com.neorig.pcbuilder.** {
    kotlinx.serialization.KSerializer serializer(...);
}
-keep,includedescriptorclasses class com.neorig.pcbuilder.**$$serializer { *; }
