cmake_minimum_required(VERSION 3.21)

project(PressureOS VERSION 1.0.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2 Sql)
qt_standard_project_setup(REQUIRES 6.4)
qt_policy(SET QTP0001 NEW)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(pressureos
    src/main.cpp
    src/backend/AppController.cpp
    src/backend/AppController.h
    src/backend/AssistantActionDispatcher.cpp
    src/backend/AssistantActionDispatcher.h
    src/backend/AssistantContextService.cpp
    src/backend/AssistantContextService.h
    src/backend/AssistantController.cpp
    src/backend/AssistantController.h
    src/backend/AssistantKnowledgeRepository.cpp
    src/backend/AssistantKnowledgeRepository.h
    src/backend/AssistantRuleEngine.cpp
    src/backend/AssistantRuleEngine.h
    src/backend/ConnectivityService.cpp
    src/backend/ConnectivityService.h
    src/backend/DatabaseService.cpp
    src/backend/DatabaseService.h
    src/backend/DeviceSimulator.cpp
    src/backend/DeviceSimulator.h
    src/backend/LegacyAsciiProtocol.cpp
    src/backend/LegacyAsciiProtocol.h
    src/backend/PinyinInputEngine.cpp
    src/backend/PinyinInputEngine.h
    src/backend/PressureSignalProcessor.cpp
    src/backend/PressureSignalProcessor.h
    src/backend/PressureProtocolV1.cpp
    src/backend/PressureProtocolV1.h
    src/backend/SerialDeviceGateway.cpp
    src/backend/SerialDeviceGateway.h
    src/backend/TaskManager.cpp
    src/backend/TaskManager.h
    src/backend/TemplateRepository.cpp
    src/backend/TemplateRepository.h
)

target_compile_definitions(pressureos PRIVATE
    PRESSUREOS_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    enable_language(RC)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deploy/windows/PressureOS.ico"
        PRESSUREOS_ICON_NATIVE)
    string(REPLACE "\\" "\\\\" PRESSUREOS_ICON_RC "${PRESSUREOS_ICON_NATIVE}")
    configure_file(
        deploy/windows/PressureOS.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/PressureOS.rc"
        @ONLY
    )
    target_sources(pressureos PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/PressureOS.rc")
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(pressureos
    URI PressureOS
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/AmbientBackdrop.qml
        qml/components/GlassPanel.qml
        qml/components/PremiumButton.qml
        qml/components/VectorIcon.qml
        qml/components/StatusChip.qml
        qml/components/AppTile.qml
        qml/components/RealtimeChart.qml
        qml/components/WaveformFocusOverlay.qml
        qml/components/MeasurementStatusItem.qml
        qml/components/FitChart.qml
        qml/components/TopStatusBar.qml
        qml/components/NavDock.qml
        qml/components/MetricCard.qml
        qml/components/MeasurementRecordDialog.qml
        qml/components/DeleteIcon.qml
        qml/components/SectionTitle.qml
        qml/components/AssistantDrawer.qml
        qml/components/AssistantNudge.qml
        qml/components/PremiumComboBox.qml
        qml/components/TouchKeyboard.qml
        qml/components/CreateTaskDialog.qml
        qml/components/QuickTaskDialog.qml
        qml/components/ZeroCalibrationDialog.qml
        qml/components/SystemPowerDialog.qml
        qml/components/SafetySelfCheckDialog.qml
        qml/pages/DesktopPage.qml
        qml/pages/MeasurementPage.qml
        qml/pages/TaskCenterPage.qml
        qml/pages/TaskRunnerPage.qml
        qml/pages/TemplateLibraryPage.qml
        qml/pages/DataStudioPage.qml
        qml/pages/DeviceCenterPage.qml
    RESOURCES
        data/assistant/knowledge_base.json
        data/input/pinyin_lexicon.tsv
        data/templates/pressure_mass_template.json
)

target_include_directories(pressureos PRIVATE src/backend)
target_link_libraries(pressureos PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
)

set_target_properties(pressureos PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS pressureos
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(pressure_filter_regression
        tests/pressure_filter_regression.cpp
        src/backend/PressureSignalProcessor.cpp
        src/backend/PressureSignalProcessor.h
    )
    target_include_directories(pressure_filter_regression PRIVATE src/backend)
    target_link_libraries(pressure_filter_regression PRIVATE Qt6::Core)
    target_compile_definitions(pressure_filter_regression PRIVATE
        PRESSUREOS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME pressure_filter_regression COMMAND pressure_filter_regression)

    qt_add_executable(pressureos_backend_smoke
        tests/backend_smoke.cpp
        src/backend/AppController.cpp
        src/backend/AppController.h
        src/backend/AssistantActionDispatcher.cpp
        src/backend/AssistantActionDispatcher.h
        src/backend/AssistantContextService.cpp
        src/backend/AssistantContextService.h
        src/backend/AssistantController.cpp
        src/backend/AssistantController.h
        src/backend/AssistantKnowledgeRepository.cpp
        src/backend/AssistantKnowledgeRepository.h
        src/backend/AssistantRuleEngine.cpp
        src/backend/AssistantRuleEngine.h
        src/backend/ConnectivityService.cpp
        src/backend/ConnectivityService.h
        src/backend/DatabaseService.cpp
        src/backend/DatabaseService.h
        src/backend/DeviceSimulator.cpp
        src/backend/DeviceSimulator.h
        src/backend/LegacyAsciiProtocol.cpp
        src/backend/LegacyAsciiProtocol.h
        src/backend/PinyinInputEngine.cpp
        src/backend/PinyinInputEngine.h
        src/backend/PressureSignalProcessor.cpp
        src/backend/PressureSignalProcessor.h
        src/backend/PressureProtocolV1.cpp
        src/backend/PressureProtocolV1.h
        src/backend/SerialDeviceGateway.cpp
        src/backend/SerialDeviceGateway.h
        src/backend/TaskManager.cpp
        src/backend/TaskManager.h
        src/backend/TemplateRepository.cpp
        src/backend/TemplateRepository.h
    )
    qt_add_resources(pressureos_backend_smoke "assistant_test_resources"
        PREFIX "/qt/qml/PressureOS"
        FILES
            data/assistant/knowledge_base.json
            data/input/pinyin_lexicon.tsv
    )
    target_include_directories(pressureos_backend_smoke PRIVATE src/backend)
    target_link_libraries(pressureos_backend_smoke PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME backend_smoke COMMAND pressureos_backend_smoke)
    if(WIN32)
        set_tests_properties(pressure_filter_regression backend_smoke PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endif()
