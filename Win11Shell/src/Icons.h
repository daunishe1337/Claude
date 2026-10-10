#pragma once
// Идентификаторы иконок. Все иконки рисуются кодом (Icons.cpp), без внешних ресурсов.

// Монохромные глифы (рисуются цветом текста).
enum class Icon
{
    None,
    // Навигация
    Search, ChevronUp, ChevronDown, ChevronLeft, ChevronRight, ArrowLeft, ArrowRight, ArrowUp, ArrowDown, Refresh,
    Close, Minimize, Maximize, Restore, Add, More, MoreVertical, Check, Hamburger, Dot, Back,
    // Действия
    Settings, Edit, Delete, Cut, Copy, Paste, Rename, Share, Sort, View, Filter, Pin, Unpin, Star, Undo, Redo,
    ZoomIn, ZoomOut, Save, Open, NewTab, Properties, CopyPath, Print, OpenWith, Select, Info,
    // Места
    Home, Gallery, Desktop, Downloads, Documents, Pictures, Music, Videos, Folder, NewFolder, File, Pc, Drive,
    Network, Globe, RecycleBin,
    // Система
    Power, Sleep, Restart, Lock, SignOut, User, Bell, Wifi, Wifi1, Wifi2, WifiOff, Ethernet, Volume0, Volume1, Volume2,
    Volume3, VolumeMute, Battery, BatteryCharging, Bluetooth, Airplane, BatterySaver, NightLight, Accessibility,
    Brightness, TaskView, Widgets, Terminal, Brush, Palette, Monitor, Keyboard, Mouse, Shield, Update, Apps, Clock,
    Calendar, Game, Language, Cast, Focus, Location, Camera, Mail, Phone, Personalize, Display, Sound, Storage,
    Multitask, Battery2, Printer,
    // Медиа и инструменты
    Play, Pause, Stop, Flag, Timer, Stopwatch, Alarm, WorldClock, Pencil, Eraser, Fill, Line, RectShape, EllipseShape,
    TextTool, Picker, Crop, RotateLeft, RotateRight, Heart, Code, Chart, Cpu, Memory, Disk, Gpu, Processes,
    Performance, Startup, History, Services, Details, Grid, List, Tiles, Eye, Lightbulb, Shuffle, Repeat, Previous,
    Next, Emoji,
    // Погода
    Sun, Moon, Cloud, PartlyCloudy, Rain, Storm, Snow, Wind, Drop, Thermometer,
};

// Цветные иконки приложений, папок и файлов.
enum class AppIcon
{
    None,
    Explorer, Notepad, Settings, Calculator, Terminal, TaskManager, Paint, Photos, Clock, Weather, Browser, Mail,
    Calendar, Store, Camera, MediaPlayer, Maps, Tips,
    ThisPC, RecycleBinEmpty, RecycleBinFull, Folder, FolderDesktop, FolderDocuments, FolderDownloads, FolderPictures,
    FolderMusic, FolderVideos, FileText, FileImage, FileAudio, FileVideo, FileGeneric, FileDoc, FileArchive, FileApp,
    DriveSystem, Drive, Network, Gallery, Home, User,
};
