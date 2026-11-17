// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

/// `kurodlg --about`: the About page every KDE application has. AboutPage reads KAboutData's application data
/// itself, which main() sets before any window opens.
Kirigami.ApplicationWindow {
    title: i18n("About kurodlg")
    width: Kirigami.Units.gridUnit * 30
    height: Kirigami.Units.gridUnit * 34
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: true

    pageStack.initialPage: FormCard.AboutPage {}
    pageStack.globalToolBar.showNavigationButtons: Kirigami.ApplicationHeaderStyle.NoNavigationButtons
}
