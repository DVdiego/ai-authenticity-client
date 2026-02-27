import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Window {
    id: root

    width: 1080
    height: 760
    visible: true
    title: qsTr("AI Authenticity Client")

    property string selectedFilePath: ""
    readonly property bool hasController: typeof appController !== "undefined" && appController !== null

    function localPathFromUrl(value) {
        var raw = String(value)
        if (raw.startsWith("file://")) {
            return decodeURIComponent(raw.substring(7))
        }
        return raw
    }

    FileDialog {
        id: fileDialog
        title: "Select image file"
        nameFilters: ["Image files (*.jpg *.jpeg *.png *.webp *.bmp)", "All files (*)"]
        onAccepted: {
            var chosen = ""
            if (fileDialog.selectedFile !== undefined && fileDialog.selectedFile !== null) {
                chosen = fileDialog.selectedFile
            } else if (fileDialog.selectedFiles !== undefined && fileDialog.selectedFiles.length > 0) {
                chosen = fileDialog.selectedFiles[0]
            }
            selectedFilePath = localPathFromUrl(chosen)
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "#0b1f33" }
            GradientStop { position: 0.5; color: "#143a52" }
            GradientStop { position: 1.0; color: "#edf3f8" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Label {
            text: "AI Authenticity Analysis"
            font.family: "Avenir Next"
            font.pixelSize: 36
            font.bold: true
            color: "#f3f7fb"
        }

        Label {
            text: "POC engines: on_device_py, on_device_native, api"
            font.family: "Avenir Next"
            font.pixelSize: 15
            color: "#c7d9e5"
        }

        Frame {
            Layout.fillWidth: true
            background: Rectangle {
                radius: 14
                color: "#f8fbfe"
                border.color: "#b8cfe0"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "Engine"
                        font.bold: true
                        color: "#233947"
                    }

                    ComboBox {
                        id: engineMode
                        model: ["on_device_py", "on_device_native", "api"]
                        currentIndex: {
                            if (!hasController) return 0
                            if (appController.engineMode === "on_device_native") return 1
                            if (appController.engineMode === "api") return 2
                            return 0
                        }
                        onActivated: if (hasController) appController.setEngineMode(currentText)
                        implicitWidth: 190
                        contentItem: Text {
                            leftPadding: 10
                            rightPadding: 10
                            text: engineMode.displayText
                            color: "#102739"
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        background: Rectangle {
                            radius: 8
                            color: "#ffffff"
                            border.color: "#9fc0d5"
                        }
                    }

                    Rectangle {
                        radius: 8
                        color: "#dbeaf4"
                        border.color: "#aac6d8"
                        Layout.alignment: Qt.AlignVCenter
                        implicitHeight: 30
                        implicitWidth: statusText.implicitWidth + 20

                        Label {
                            id: statusText
                            anchors.centerIn: parent
                            text: "active: " + (hasController ? appController.engineName : "none")
                            color: "#2a4f67"
                            font.bold: true
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        id: selectFileButton
                        text: "Select File"
                        font.bold: true
                        onClicked: fileDialog.open()
                        background: Rectangle {
                            radius: 8
                            color: "#1e88c7"
                            border.color: "#176b9c"
                        }
                        contentItem: Text {
                            text: selectFileButton.text
                            color: "#ffffff"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        id: executeButton
                        text: "Execute"
                        font.bold: true
                        enabled: selectedFilePath.length > 0
                        onClicked: if (hasController) appController.analyzeFile(selectedFilePath)
                        background: Rectangle {
                            radius: 8
                            color: executeButton.enabled ? "#0f6b45" : "#b6c7d1"
                            border.color: executeButton.enabled ? "#0a5234" : "#9caeb9"
                        }
                        contentItem: Text {
                            text: executeButton.text
                            color: executeButton.enabled ? "#ffffff" : "#f5f8fa"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        id: compareButton
                        text: "Compare"
                        font.bold: true
                        enabled: selectedFilePath.length > 0
                        onClicked: if (hasController) appController.compareEngines(selectedFilePath)
                        background: Rectangle {
                            radius: 8
                            color: compareButton.enabled ? "#6b46c1" : "#b6c7d1"
                            border.color: compareButton.enabled ? "#5938a0" : "#9caeb9"
                        }
                        contentItem: Text {
                            text: compareButton.text
                            color: compareButton.enabled ? "#ffffff" : "#f5f8fa"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#ffffff"
                    border.color: selectedFilePath.length > 0 ? "#74b0d0" : "#d0dde6"
                    implicitHeight: 54

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10

                        Label {
                            text: selectedFilePath.length > 0 ? selectedFilePath : "No file selected"
                            color: selectedFilePath.length > 0 ? "#1f3646" : "#7b8b98"
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        Button {
                            id: clearButton
                            text: "Clear"
                            enabled: selectedFilePath.length > 0
                            onClicked: selectedFilePath = ""
                            background: Rectangle {
                                radius: 8
                                color: "#ffffff"
                                border.color: "#9db5c6"
                            }
                            contentItem: Text {
                                text: clearButton.text
                                color: "#294354"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 8
                    color: "#eef5fb"
                    border.color: "#c8deed"
                    implicitHeight: 44

                    Label {
                        anchors.fill: parent
                        anchors.margins: 10
                        text: hasController ? appController.statusMessage : "Controller not connected in preview mode."
                        wrapMode: Text.WordWrap
                        color: "#304a5b"
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                radius: 14
                color: "#ffffff"
                border.color: "#c2d7e6"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                Label {
                    text: "Result"
                    font.pixelSize: 24
                    font.bold: true
                    color: "#1d3444"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: "#f3f8fc"
                        border.color: "#c4d9e8"
                        implicitHeight: 100

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Label { text: "Decision"; font.bold: true; color: "#2b4a5f" }
                            Label { text: hasController ? appController.decision : "-"; font.pixelSize: 18; font.bold: true; color: "#102739" }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: "#f3f8fc"
                        border.color: "#c4d9e8"
                        implicitHeight: 100

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Label { text: "Probability (AI)"; font.bold: true; color: "#2b4a5f" }
                            Label { text: hasController ? Number(appController.aiProbability).toFixed(2) + " %" : "0.00 %"; font.pixelSize: 18; font.bold: true; color: "#102739" }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: "#f3f8fc"
                        border.color: "#c4d9e8"
                        implicitHeight: 100

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Label { text: "Media"; font.bold: true; color: "#2b4a5f" }
                            Label { text: hasController ? appController.mediaType : "-"; font.pixelSize: 18; font.bold: true; color: "#102739" }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    color: "#f8fbfe"
                    border.color: "#d4e2ed"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 12

                        Label {
                            width: parent.width
                            text: {
                                if (!hasController)
                                    return "Run the app executable to see live output."
                                if (appController.comparisonReport.length > 0)
                                    return appController.comparisonReport
                                return appController.explanation
                            }
                            wrapMode: Text.WordWrap
                            color: "#334e60"
                            lineHeight: 1.2
                        }
                    }
                }
            }
        }
    }
}
