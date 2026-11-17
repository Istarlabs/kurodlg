// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.dateandtime as DateAndTime

// The module's own C++ types (FormController) are registered when it is imported by name.
import net.kurokyuu.kurodlg

/// A window built from the form's fields, one row per field.
QQC2.ApplicationWindow {
    id: window

    required property FormController controller

    readonly property int margin: Kirigami.Units.largeSpacing

    // The calendar is a popup inside this window, so a window shorter than the calendar cuts it off. It is
    // 20 grid units tall (see DatePopup below); the rest is room around it.
    readonly property real calendarHeight: Kirigami.Units.gridUnit * 22
    readonly property bool hasDate: controller.fields.some(field => field.type === "date")

    width: Math.max(Kirigami.Units.gridUnit * 24, layout.implicitWidth + 2 * margin)
    height: Math.max(layout.implicitHeight + 2 * margin, hasDate ? calendarHeight : 0)
    minimumWidth: layout.Layout.minimumWidth + 2 * margin
    minimumHeight: Math.max(layout.Layout.minimumHeight + 2 * margin, hasDate ? calendarHeight : 0)
    visible: true

    onClosing: controller.reject()

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: window.margin
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: window.controller.text
            wrapMode: Text.Wrap
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            Repeater {
                model: window.controller.fields

                delegate: Loader {
                    id: row

                    required property int index
                    required property var modelData

                    readonly property string value: window.controller.values[index]

                    // A checkbox carries its own label, so a second one in the form's left column would
                    // only repeat it.
                    Kirigami.FormData.label: modelData.type === "check" ? "" : modelData.label + ":"

                    sourceComponent: {
                        switch (modelData.type) {
                        case "password":
                            return passwordField;
                        case "int":
                            return spinBox;
                        case "check":
                            return checkBox;
                        case "combo":
                            return comboBox;
                        case "file":
                        case "dir":
                            return pathField;
                        case "date":
                            return dateButton;
                        }

                        return textField;
                    }

                    // The first row takes the keyboard.
                    onLoaded: if (index === 0) item.forceActiveFocus()

                    Component {
                        id: textField

                        QQC2.TextField {
                            text: row.value
                            onTextEdited: window.controller.setValue(row.index, text)
                            onAccepted: window.controller.accept()
                        }
                    }

                    Component {
                        id: passwordField

                        Kirigami.PasswordField {
                            text: row.value
                            onTextEdited: window.controller.setValue(row.index, text)
                            onAccepted: window.controller.accept()
                        }
                    }

                    Component {
                        id: spinBox

                        QQC2.SpinBox {
                            editable: true
                            from: row.modelData.minimum
                            to: row.modelData.maximum
                            value: parseInt(row.value)
                            onValueModified: window.controller.setValue(row.index, String(value))
                        }
                    }

                    Component {
                        id: checkBox

                        QQC2.CheckBox {
                            text: row.modelData.label
                            checked: row.value === "on"
                            onToggled: window.controller.setValue(row.index, checked ? "on" : "off")
                        }
                    }

                    Component {
                        id: comboBox

                        QQC2.ComboBox {
                            model: row.modelData.choices
                            currentIndex: row.modelData.choices.indexOf(row.value)
                            onActivated: index => window.controller.setValue(row.index, row.modelData.choices[index])
                        }
                    }

                    // A file or a folder: the path as text, which need not exist yet, and a button to pick one.
                    Component {
                        id: pathField

                        RowLayout {
                            function forceActiveFocus(): void {
                                field.forceActiveFocus();
                            }

                            QQC2.TextField {
                                id: field

                                Layout.fillWidth: true
                                text: row.value
                                onTextEdited: window.controller.setValue(row.index, text)
                                onAccepted: window.controller.accept()
                            }

                            QQC2.Button {
                                icon.name: row.modelData.type === "dir" ? "folder-open" : "document-open"
                                text: i18n("Browse…")
                                display: QQC2.AbstractButton.IconOnly
                                QQC2.ToolTip.text: text
                                QQC2.ToolTip.visible: hovered

                                onClicked: row.modelData.type === "dir" ? folderDialog.open() : fileDialog.open()
                            }

                            FileDialog {
                                id: fileDialog

                                title: row.modelData.label
                                selectedFile: "file://" + row.value
                                onAccepted: window.controller.setValue(row.index,
                                                                       selectedFile.toString().replace("file://", ""))
                            }

                            FolderDialog {
                                id: folderDialog

                                title: row.modelData.label
                                selectedFolder: "file://" + row.value
                                onAccepted: window.controller.setValue(row.index,
                                                                       selectedFolder.toString().replace("file://", ""))
                            }
                        }
                    }

                    Component {
                        id: dateButton

                        QQC2.Button {
                            id: dateEditor

                            // The value is an ISO date; built from its parts, because a date-only string is read as
                            // UTC and would show the previous day west of Greenwich.
                            readonly property date date: {
                                const parts = row.value.split("-");
                                return new Date(parseInt(parts[0]), parseInt(parts[1]) - 1, parseInt(parts[2]));
                            }

                            icon.name: "view-calendar"
                            text: date.toLocaleDateString(Qt.locale(), Locale.LongFormat)
                            onClicked: {
                                popup.value = date;
                                popup.open();
                            }

                            DateAndTime.DatePopup {
                                id: popup

                                parent: QQC2.Overlay.overlay
                                x: Math.round((parent.width - width) / 2)
                                y: Math.round((parent.height - height) / 2)
                                width: Math.min(Kirigami.Units.gridUnit * 20, parent.width - 2 * Kirigami.Units.gridUnit)
                                height: Kirigami.Units.gridUnit * 20
                                modal: true

                                onAccepted: window.controller.setValue(row.index, Qt.formatDate(value, "yyyy-MM-dd"))
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        QQC2.DialogButtonBox {
            Layout.fillWidth: true
            standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel

            onAccepted: window.controller.accept()
            onRejected: window.controller.reject()
        }
    }
}
