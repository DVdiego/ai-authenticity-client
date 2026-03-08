import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import QtQuick.Dialogs

Window {
    id: root

    width: 1080
    height: 820
    visible: true
    title: qsTr("AI Authenticity Client")

    property string selectedFilePath: ""
    readonly property string legalDocsPath: hasController ? appController.legalDocsPath : ""
    readonly property string repoUrl: "https://github.com/DVdiego/ai-authenticity-client"
    readonly property string releasesUrl: repoUrl + "/releases"
    readonly property string legalUrl: repoUrl + "/tree/develop/legal"
    readonly property string legalSummaryText: "Research-only build. Non-commercial use only. Results are indicative and must not be used as forensic proof, compliance evidence, or for high-impact decisions."
    readonly property string legalFullText:
        "AI Authenticity Client - distribution notice\n\n" +
        "Intended use\n" +
        "- Educational, research, and internal evaluation use only.\n" +
        "- No commercial use, resale, hosting, sublicensing, or paid service use without explicit written authorization.\n\n" +
        "Model and output limitations\n" +
        "- The application provides probabilistic, non-deterministic indicators.\n" +
        "- Outputs are not forensic proof, not legal evidence, and not a substitute for expert review.\n" +
        "- Do not use the results as the sole basis for disciplinary, employment, admissions, law-enforcement, compliance, or other high-impact decisions.\n\n" +
        "Liability and scope\n" +
        "- The software is provided as is, without warranties.\n" +
        "- The user is responsible for lawful use, dataset rights, and compliance with the applicable jurisdiction.\n" +
        "- If the software is redistributed outside the authors' control, downstream distributors assume responsibility for their own deployment, claims, and regulatory compliance.\n\n" +
        "Distribution identity\n" +
        "- " + (hasController ? appController.buildSummary : "Build metadata unavailable") + "\n" +
        "- " + (hasController ? appController.licenseSummary : "License metadata unavailable") + "\n\n" +
        "Public references\n" +
        "- Repository: " + repoUrl + "\n" +
        "- Releases: " + releasesUrl + "\n" +
        "- Legal documents: " + legalUrl
    readonly property bool hasController: typeof appController !== "undefined" && appController !== null

    function localPathFromUrl(value) {
        var raw = String(value)
        if (raw.startsWith("file://")) {
            return decodeURIComponent(raw.substring(7))
        }
        return raw
    }

    Settings {
        id: legalSettings
        category: "legal"
        property bool accepted: false
    }

    Component.onCompleted: {
        if (!legalSettings.accepted) {
            legalDialog.open()
        }
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

    Dialog {
        id: legalDialog
        title: "Legal Notice"
        modal: true
        dim: true
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        width: Math.min(root.width - 80, 760)
        height: Math.min(root.height - 80, 560)
        closePolicy: legalSettings.accepted ? Popup.CloseOnEscape | Popup.CloseOnPressOutside : Popup.NoAutoClose

        background: Rectangle {
            radius: 14
            color: "#f8fbfe"
            border.color: "#b8cfe0"
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: legalSummaryText
                wrapMode: Text.WordWrap
                color: "#243b4c"
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 10
                color: hasController && appController.licenseValid ? "#eef8f1" : "#fff2f0"
                border.color: hasController && appController.licenseValid ? "#7eb791" : "#d48b81"
                implicitHeight: Math.max(58, legalMetaText.implicitHeight + 16)

                Text {
                    id: legalMetaText
                    anchors.fill: parent
                    anchors.margins: 8
                    text: (hasController ? appController.buildSummary : "Build metadata unavailable") +
                          "\n" +
                          (hasController ? appController.licenseSummary : "License metadata unavailable")
                    wrapMode: Text.WordWrap
                    color: "#1f3646"
                    font.pixelSize: 12
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    text: legalFullText
                    readOnly: true
                    wrapMode: Text.Wrap
                    color: "#1f3646"
                    background: Rectangle {
                        radius: 10
                        color: "#ffffff"
                        border.color: "#d0dde6"
                    }
                }
            }
        }

        footer: RowLayout {
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                text: legalSettings.accepted ? "Close" : "Decline"
                onClicked: {
                    if (legalSettings.accepted) {
                        legalDialog.close()
                    } else {
                        Qt.quit()
                    }
                }
            }

            Button {
                text: legalSettings.accepted ? "Accepted" : "Accept"
                enabled: !legalSettings.accepted
                onClicked: {
                    legalSettings.accepted = true
                    legalDialog.close()
                }
            }
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
        anchors.margins: 18
        spacing: 10

        Label {
            text: "AI Authenticity Analysis"
            font.family: "Avenir Next"
            font.pixelSize: 34
            font.bold: true
            color: "#f3f7fb"
        }

        Label {
            text: hasController && appController.showDevOptions
                  ? "POC engines: on_device_py, on_device_native, api"
                  : "Distribution build: native analysis"
            font.family: "Avenir Next"
            font.pixelSize: 14
            color: "#c7d9e5"
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 12
            color: "#f6e7b8"
            border.color: "#d4b45c"
            implicitHeight: Math.max(44, legalBannerText.implicitHeight + 16)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    id: legalBannerText
                    Layout.fillWidth: true
                    text: legalSummaryText
                    wrapMode: Text.WordWrap
                    color: "#5a4200"
                    font.pixelSize: 12
                }

                Button {
                    text: "Legal"
                    onClicked: legalDialog.open()
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 270
            background: Rectangle {
                radius: 14
                color: "#f8fbfe"
                border.color: "#b8cfe0"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Engine"
                        font.bold: true
                        color: "#233947"
                        visible: hasController && appController.showDevOptions
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
                        implicitHeight: 36
                        visible: hasController && appController.showDevOptions
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
                        implicitHeight: 36
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
                        implicitHeight: 36
                        enabled: selectedFilePath.length > 0
                                 && legalSettings.accepted
                                 && hasController
                                 && appController.licenseValid
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
                        implicitHeight: 36
                        enabled: selectedFilePath.length > 0
                                 && legalSettings.accepted
                                 && hasController
                                 && appController.licenseValid
                        visible: hasController && appController.showDevOptions
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
                    border.color: "#d0dde6"
                    implicitHeight: 48

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: "shared model"
                            font.bold: true
                            color: "#294354"
                        }

                        ComboBox {
                            id: pyModelCombo
                            Layout.fillWidth: true
                            model: hasController ? appController.pyModelOptions : []
                            currentIndex: hasController ? appController.pyModelIndex : -1
                            enabled: model.length > 0
                            implicitHeight: 32
                            onActivated: if (hasController) appController.setPyModelIndex(currentIndex)
                            contentItem: Text {
                                leftPadding: 10
                                rightPadding: 10
                                text: pyModelCombo.displayText
                                color: pyModelCombo.enabled ? "#102739" : "#90a3af"
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                radius: 8
                                color: "#ffffff"
                                border.color: "#9fc0d5"
                            }
                        }

                        Button {
                            id: refreshModelsButton
                            text: "Refresh"
                            implicitHeight: 32
                            onClicked: if (hasController) appController.refreshPyModelOptions()
                            background: Rectangle {
                                radius: 8
                                color: "#ffffff"
                                border.color: "#9db5c6"
                            }
                            contentItem: Text {
                                text: refreshModelsButton.text
                                color: "#294354"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#f7fbfe"
                    border.color: "#d0dde6"
                    implicitHeight: 36

                    Label {
                        anchors.fill: parent
                        anchors.margins: 8
                        text: hasController && appController.pyModelPath.length > 0
                              ? (appController.showDevOptions
                                    ? "py: " + appController.pyModelPath + " | native: " + appController.nativeModelVersion
                                    : "native package: " + appController.nativeModelVersion)
                              : "No shared model selected"
                        color: hasController && appController.pyModelPath.length > 0 ? "#486172" : "#7b8b98"
                        elide: Text.ElideMiddle
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#f7fbfe"
                    border.color: hasController && appController.licenseValid ? "#cfe0ea" : "#e0bdb7"
                    implicitHeight: Math.max(42, buildInfoText.implicitHeight + 16)

                    Text {
                        id: buildInfoText
                        anchors.fill: parent
                        anchors.margins: 8
                        text: hasController
                              ? (appController.buildSummary + "\n" + appController.licenseSummary)
                              : "Build metadata unavailable"
                        wrapMode: Text.WordWrap
                        color: "#486172"
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#ffffff"
                    border.color: selectedFilePath.length > 0 ? "#74b0d0" : "#d0dde6"
                    implicitHeight: 48

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8

                        Label {
                            text: selectedFilePath.length > 0 ? selectedFilePath : "No file selected"
                            color: selectedFilePath.length > 0 ? "#1f3646" : "#7b8b98"
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        Button {
                            id: clearButton
                            text: "Clear"
                            implicitHeight: 32
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
                    implicitHeight: Math.max(36, feedbackText.implicitHeight + 16)

                    Text {
                        id: feedbackText
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        text: hasController ? appController.statusMessage : "Controller not connected in preview mode."
                        wrapMode: Text.WordWrap
                        color: "#304a5b"
                        font.pixelSize: 12
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            background: Rectangle {
                radius: 14
                color: "#ffffff"
                border.color: "#c2d7e6"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Label {
                    text: "Result"
                    font.pixelSize: 22
                    font.bold: true
                    color: "#1d3444"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 190
                        Layout.minimumWidth: 150
                        Layout.maximumWidth: 210
                        radius: 12
                        color: "#f3f8fc"
                        border.color: "#c4d9e8"
                        implicitHeight: 64

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Label { text: "Decision"; font.pixelSize: 11; font.bold: true; color: "#2b4a5f" }
                            Label { text: hasController ? appController.decision : "-"; font.pixelSize: 15; font.bold: true; color: "#102739" }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 190
                        Layout.minimumWidth: 150
                        Layout.maximumWidth: 210
                        radius: 12
                        color: "#f3f8fc"
                        border.color: "#c4d9e8"
                        implicitHeight: 64

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Label { text: "Probability (AI)"; font.pixelSize: 11; font.bold: true; color: "#2b4a5f" }
                            Label { text: hasController ? Number(appController.aiProbability).toFixed(2) + " %" : "0.00 %"; font.pixelSize: 15; font.bold: true; color: "#102739" }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 190
                        Layout.minimumWidth: 150
                        Layout.maximumWidth: 210
                        radius: 12
                        color: "#f3f8fc"
                        border.color: "#c4d9e8"
                        implicitHeight: 64

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Label { text: "Media"; font.pixelSize: 11; font.bold: true; color: "#2b4a5f" }
                            Label { text: hasController ? appController.mediaType : "-"; font.pixelSize: 15; font.bold: true; color: "#102739" }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    color: "#f8fbfe"
                    border.color: "#d4e2ed"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 10
                        clip: true

                        Text {
                            width: Math.max(0, parent.width)
                            text: {
                                if (!hasController)
                                    return "Run the app executable to see live output."
                                if (appController.comparisonReport.length > 0)
                                    return appController.comparisonReport
                                return appController.explanation
                            }
                            wrapMode: Text.WordWrap
                            color: "#334e60"
                            font.pixelSize: 12
                            lineHeight: 1.15
                        }
                    }
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 260
            background: Rectangle {
                radius: 14
                color: "#ffffff"
                border.color: "#c2d7e6"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Label {
                    text: "History"
                    font.pixelSize: 22
                    font.bold: true
                    color: "#1d3444"
                }

                TabBar {
                    id: historyTabs
                    Layout.fillWidth: true
                    implicitHeight: 36

                    TabButton { text: "Execute" }
                    TabButton { text: "Compare" }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: historyTabs.currentIndex

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Text {
                            width: Math.max(0, parent.width)
                            text: hasController && appController.analysisHistory.length > 0
                                  ? appController.analysisHistory
                                  : "No execute history yet."
                            wrapMode: Text.WordWrap
                            color: "#334e60"
                            font.pixelSize: 12
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Text {
                            width: Math.max(0, parent.width)
                            text: hasController && appController.comparisonHistory.length > 0
                                  ? appController.comparisonHistory
                                  : "No compare history yet."
                            wrapMode: Text.WordWrap
                            color: "#334e60"
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }
}
