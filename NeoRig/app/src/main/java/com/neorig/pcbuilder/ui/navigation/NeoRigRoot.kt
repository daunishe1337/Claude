package com.neorig.pcbuilder.ui.navigation

import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.navigation
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.neorig.pcbuilder.R
import com.neorig.pcbuilder.di.AppContainer
import com.neorig.pcbuilder.ui.build.BuildScreen
import com.neorig.pcbuilder.ui.build.BuilderEvent
import com.neorig.pcbuilder.ui.build.BuilderViewModel
import com.neorig.pcbuilder.ui.saved.CompareScreen
import com.neorig.pcbuilder.ui.saved.SavedBuildsScreen
import com.neorig.pcbuilder.ui.saved.SavedBuildsViewModel
import com.neorig.pcbuilder.ui.settings.SettingsScreen
import com.neorig.pcbuilder.ui.settings.SettingsViewModel
import com.neorig.pcbuilder.ui.theme.NeoColors
import com.neorig.pcbuilder.ui.theme.NeoIcons
import com.neorig.pcbuilder.ui.viewer.ViewerScreen
import com.neorig.pcbuilder.ui.wizard.BudgetScreen
import com.neorig.pcbuilder.ui.wizard.PreferencesScreen

/** Маршруты навигации. */
object Routes {
    const val BUILDER = "builder"
    const val BUDGET = "budget"
    const val PREFERENCES = "preferences"
    const val BUILD = "build"
    const val VIEWER = "viewer"
    const val SAVED = "saved"
    const val SETTINGS = "settings"
    const val COMPARE = "compare/{a}/{b}"

    fun compare(a: Long, b: Long) = "compare/$a/$b"
}

/** Пункт нижней навигации. */
private data class TabItem(val route: String, val labelRes: Int, val icon: ImageVector)

/**
 * Корень интерфейса: Scaffold с нижней навигацией (Сборка / 3D / Мои сборки / Настройки)
 * и NavHost. ViewModel создаются здесь, на уровне Activity, и передаются экранам.
 */
@Composable
fun NeoRigRoot(container: AppContainer, onLanguageChanged: () -> Unit) {
    val navController = rememberNavController()
    val builderViewModel: BuilderViewModel = viewModel(factory = container.viewModelFactory)
    val savedViewModel: SavedBuildsViewModel = viewModel(factory = container.viewModelFactory)
    val settingsViewModel: SettingsViewModel = viewModel(factory = container.viewModelFactory)
    val settings by settingsViewModel.settings.collectAsStateWithLifecycle()
    val builderState by builderViewModel.state.collectAsStateWithLifecycle()
    val snackbarHostState = remember { SnackbarHostState() }
    val context = LocalContext.current

    // При запуске с уже собранным ПК (восстановленная сессия) сразу открываем экран сборки
    var sessionRestored by rememberSaveable { mutableStateOf(false) }
    LaunchedEffect(builderState.loading) {
        if (!builderState.loading && !sessionRestored) {
            sessionRestored = true
            if (builderState.build != null) {
                navController.navigate(Routes.BUILD) { launchSingleTop = true }
            }
        }
    }

    // События конструктора: переход к сборке, сообщение о сохранении
    LaunchedEffect(builderViewModel) {
        builderViewModel.events.collect { event ->
            when (event) {
                BuilderEvent.OpenBuild -> navController.navigate(Routes.BUILD) {
                    popUpTo(Routes.BUDGET)
                    launchSingleTop = true
                }
                is BuilderEvent.Saved -> snackbarHostState.showSnackbar(
                    context.getString(R.string.saved_toast, event.name)
                )
            }
        }
    }

    val tabs = listOf(
        TabItem(Routes.BUILDER, R.string.nav_build, NeoIcons.Tune),
        TabItem(Routes.VIEWER, R.string.nav_3d, NeoIcons.Cube),
        TabItem(Routes.SAVED, R.string.nav_saved, NeoIcons.Bookmark),
        TabItem(Routes.SETTINGS, R.string.nav_settings, Icons.Filled.Settings),
    )

    Scaffold(
        containerColor = NeoColors.Background,
        snackbarHost = { SnackbarHost(snackbarHostState) },
        bottomBar = { BottomBar(navController, tabs) },
    ) { padding ->
        NavHost(
            navController = navController,
            startDestination = Routes.BUILDER,
            modifier = Modifier.padding(padding),
        ) {
            navigation(startDestination = Routes.BUDGET, route = Routes.BUILDER) {
                composable(Routes.BUDGET) {
                    BudgetScreen(builderViewModel, onNext = { navController.navigate(Routes.PREFERENCES) })
                }
                composable(Routes.PREFERENCES) {
                    PreferencesScreen(builderViewModel, onBack = { navController.popBackStack() })
                }
                composable(Routes.BUILD) {
                    BuildScreen(
                        viewModel = builderViewModel,
                        settings = settings,
                        onOpen3d = { navController.navigateToTab(Routes.VIEWER) },
                        onChangePreferences = {
                            navController.navigate(Routes.PREFERENCES) { launchSingleTop = true }
                        },
                        onStart = { navController.navigate(Routes.BUDGET) { launchSingleTop = true } },
                    )
                }
            }
            composable(Routes.VIEWER) {
                ViewerScreen(
                    builderViewModel = builderViewModel,
                    settingsViewModel = settingsViewModel,
                    onAssemble = { navController.navigateToTab(Routes.BUILDER) },
                )
            }
            composable(Routes.SAVED) {
                SavedBuildsScreen(
                    viewModel = savedViewModel,
                    onOpen = { saved ->
                        builderViewModel.load(saved)
                        navController.navigate(Routes.BUILD) {
                            popUpTo(navController.graph.findStartDestination().id)
                            launchSingleTop = true
                        }
                    },
                    onCompare = { a, b -> navController.navigate(Routes.compare(a, b)) },
                )
            }
            composable(Routes.SETTINGS) {
                SettingsScreen(settingsViewModel, onLanguageChanged = onLanguageChanged)
            }
            composable(
                route = Routes.COMPARE,
                arguments = listOf(
                    navArgument("a") { type = NavType.LongType },
                    navArgument("b") { type = NavType.LongType },
                ),
            ) { entry ->
                CompareScreen(
                    viewModel = savedViewModel,
                    firstId = entry.arguments?.getLong("a") ?: 0L,
                    secondId = entry.arguments?.getLong("b") ?: 0L,
                    onBack = { navController.popBackStack() },
                )
            }
        }
    }
}

@Composable
private fun BottomBar(navController: NavHostController, tabs: List<TabItem>) {
    val backStackEntry by navController.currentBackStackEntryAsState()
    val destination = backStackEntry?.destination
    NavigationBar(containerColor = NeoColors.Surface) {
        tabs.forEach { tab ->
            val selected = destination?.hierarchy?.any { it.route == tab.route } == true
            NavigationBarItem(
                selected = selected,
                onClick = { navController.navigateToTab(tab.route) },
                icon = { Icon(tab.icon, contentDescription = null) },
                label = { Text(stringResource(tab.labelRes)) },
                colors = NavigationBarItemDefaults.colors(
                    selectedIconColor = NeoColors.Teal,
                    selectedTextColor = NeoColors.Teal,
                    indicatorColor = NeoColors.Violet.copy(alpha = 0.22f),
                    unselectedIconColor = NeoColors.TextMuted,
                    unselectedTextColor = NeoColors.TextMuted,
                ),
            )
        }
    }
}

/** Переход на вкладку с сохранением состояния остальных вкладок. */
private fun NavHostController.navigateToTab(route: String) {
    navigate(route) {
        popUpTo(graph.findStartDestination().id) { saveState = true }
        launchSingleTop = true
        restoreState = true
    }
}
