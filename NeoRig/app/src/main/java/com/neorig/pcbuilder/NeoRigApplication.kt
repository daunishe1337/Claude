package com.neorig.pcbuilder

import android.app.Application
import com.neorig.pcbuilder.di.AppContainer

class NeoRigApplication : Application() {

    /** Контейнер зависимостей — создаётся лениво при первом обращении. */
    val container: AppContainer by lazy { AppContainer(this) }
}
