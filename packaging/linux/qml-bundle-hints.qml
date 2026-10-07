// Not part of the application. linuxdeploy's Qt plugin scans QML sources to decide which QML
// modules to bundle; the non-native file dialog (used whenever the desktop provides no native
// dialog) imports these from inside Qt's own resources, where the scanner cannot see them.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Qt.labs.folderlistmodel

Item {}
