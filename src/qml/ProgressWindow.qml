// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

// The module's own C++ types (ProgressController) are registered when it is imported by name.
import net.kurokyuu.kurodlg

/// The progress window and, with the bar switched off, the console output window: a bar with a
/// "Details" area underneath that holds the command's output.
QQC2.ApplicationWindow {
    id: window

    required property ProgressController controller

    readonly property int margin: Kirigami.Units.largeSpacing

    // The output is shown in a text window always, in a progress window only while Details is open.
    readonly property bool outputShown: controller.detailsExpanded && (controller.textView || controller.detailsAvailable)

    title: controller.title
    width: controller.initialWidth
    height: controller.initialHeight
    minimumWidth: Kirigami.Units.gridUnit * 18
    minimumHeight: layout.Layout.minimumHeight + 2 * margin
    visible: true

    onClosing: controller.reject()

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: window.margin
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            visible: text.length > 0
            level: 2
            text: window.controller.header
            wrapMode: Text.Wrap
        }

        Kirigami.SelectableLabel {
            Layout.fillWidth: true
            text: window.controller.text
            wrapMode: Text.Wrap
        }

        QQC2.ProgressBar {
            Layout.fillWidth: true
            visible: window.controller.progressVisible
            from: 0
            to: window.controller.maximum
            value: window.controller.value
        }

        QQC2.ToolButton {
            visible: !window.controller.textView && window.controller.detailsAvailable
            icon.name: window.controller.detailsExpanded ? "arrow-down" : "arrow-right"
            text: i18n("Details")
            onClicked: window.controller.detailsExpanded = !window.controller.detailsExpanded
        }

        QQC2.ScrollView {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: visible ? Kirigami.Units.gridUnit * 5 : 0
            visible: window.outputShown

            QQC2.TextArea {
                id: output

                readOnly: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                font: Kirigami.Theme.fixedWidthFont

                // Not a binding: output() has no change signal, and later text arrives through outputAppended.
                Component.onCompleted: text = window.controller.output()
            }
        }

        // A spacer takes the room when the output is not shown, so the buttons stay at the bottom of a window
        // that is taller than it needs to be.
        Item {
            Layout.fillHeight: !window.outputShown
        }

        RowLayout {
            Layout.fillWidth: true

            // On the left, apart from Cancel/Close: it acts on the text, not on the window or the job.
            QQC2.Button {
                id: copyButton

                property bool copied: false

                visible: window.controller.copyVisible
                icon.name: "edit-copy"
                text: copied ? i18n("Copied") : i18n("Copy")
                QQC2.ToolTip.text: i18n("Copy the whole output to the clipboard")
                QQC2.ToolTip.visible: hovered

                onClicked: {
                    window.controller.copyOutput();
                    copied = true;
                    copiedTimer.restart();
                }

                Timer {
                    id: copiedTimer

                    // Long enough to be read after the click, short enough that the button is back before it is
                    // wanted again.
                    interval: 1500
                    onTriggered: copyButton.copied = false
                }
            }

            Item {
                Layout.fillWidth: true
            }

            QQC2.Button {
                visible: window.controller.cancelVisible || window.controller.finished
                enabled: window.controller.finished || !window.controller.cancelled
                icon.name: window.controller.finished ? "dialog-close" : "dialog-cancel"
                text: {
                    if (window.controller.finished) {
                        return i18n("Close");
                    }

                    if (window.controller.cancelled) {
                        return i18n("Cancelling…");
                    }

                    return window.controller.textView ? i18n("Stop") : i18n("Cancel");
                }

                onClicked: window.controller.finished ? window.controller.accept() : window.controller.cancel()
            }
        }
    }

    Connections {
        target: window.controller

        function onOutputAppended(text: string): void {
            const flickable = scroll.contentItem;

            // Scrolling back to read something is an instruction to stop following the tail.
            const followTail = flickable.contentY + flickable.height >= flickable.contentHeight - 1;

            output.insert(output.length, text);

            if (followTail) {
                Qt.callLater(() => flickable.contentY = Math.max(0, flickable.contentHeight - flickable.height));
            }
        }

        function onOutputReset(): void {
            output.text = window.controller.output();
        }

        function onResizeRequested(width: int, height: int): void {
            window.width = width;
            window.height = height;
        }

        function onDetailsExpandedChanged(): void {
            // Qt grows a window to honour a larger minimum but never shrinks one back.
            if (!window.controller.textView && !window.controller.detailsExpanded) {
                Qt.callLater(() => window.height = window.minimumHeight);
            }
        }
    }
}
